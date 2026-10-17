// =====================================================================
//  LoRa Local-First Control Platform - NODE (Arduino Mega 2560)
//  Phase 4: receive commands, switch relays, ACK, send telemetry
//
//  Radio reliability fixes:
//   - Removed LoRa.receive() — let parsePacket() handle mode switching
//     on its own (the two conflict through slow SPI)
//   - Hard reset of the SX1278 via RST pin in the radio watchdog
//   - Small delay after endPacket() to let the module settle
//   - Reduced TX frequency: telemetry every 10s, heartbeat every 30s
//   - AVR watchdog as a last resort
// =====================================================================

#include <SPI.h>
#include <LoRa.h>
#include <avr/wdt.h>
#include "protocol.h"

// --- Relay pins ---
const uint8_t RELAY_PIN[] = { 22, 23, 24, 25 };
const uint8_t RELAY_COUNT = sizeof(RELAY_PIN) / sizeof(RELAY_PIN[0]);
const bool RELAY_ACTIVE_LOW = false;  // this module: HIGH = relay ON

// --- Relay state ---
bool relayState[4] = { false, false, false, false };

// --- LoRa pins ---
const int PIN_NSS  = 53;
const int PIN_RST  = 9;
const int PIN_DIO0 = 2;

const long FREQUENCY = 433E6;
const int  SPREADING = 9;
const long BANDWIDTH = 125E3;
const int  TX_POWER  = 10;

// --- Timing (reduced TX frequency = less chance of getting stuck) ---
const uint32_t TELEMETRY_INTERVAL_MS = 10000;   // was 5s, now 10s
const uint32_t HEARTBEAT_INTERVAL_MS = 30000;   // was 10s, now 30s
const uint32_t FAILSAFE_TIMEOUT_MS   = 60000;   // extended to match slower heartbeat
const uint32_t RADIO_WATCHDOG_MS     = 15000;

uint32_t lastTelemetry = 0;
uint32_t lastHeartbeat = 0;
uint32_t lastGatewayHeard = 0;
uint32_t lastPacketReceived = 0;
bool failsafeActive = false;

const int PIN_LED = 13;

// =====================================================================
//  Relay control
// =====================================================================
void setRelay(uint8_t idx, bool on) {
  if (idx >= RELAY_COUNT) return;
  relayState[idx] = on;
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(RELAY_PIN[idx], on ? LOW : HIGH);
  } else {
    digitalWrite(RELAY_PIN[idx], on ? HIGH : LOW);
  }
}

void allRelaysOff() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) setRelay(i, false);
}

uint8_t relayStateBits() {
  uint8_t bits = 0;
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    if (relayState[i]) bits |= (1 << i);
  }
  return bits;
}

// =====================================================================
//  Transmit helper: send, then let the radio settle
// =====================================================================
void loRaSend(const uint8_t *buf, uint8_t len) {
  LoRa.beginPacket();
  LoRa.write(buf, len);
  LoRa.endPacket();
  delay(10);   // let the module finish switching back to standby
  // Do NOT call LoRa.receive() here — parsePacket() handles it
}

// =====================================================================
//  Hard reset the SX1278 via the RST pin, then re-init
//  This forces the chip into a known state no matter what happened.
// =====================================================================
void hardResetRadio() {
  Serial.println("[Radio] hard reset...");
  // Pull RST low for 10ms — this resets all internal registers
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  delay(10);

  LoRa.setSPIFrequency(1E6);
  LoRa.setPins(PIN_NSS, PIN_RST, PIN_DIO0);
  if (!LoRa.begin(FREQUENCY)) {
    Serial.println("[Radio] FAIL after reset!");
    return;
  }
  LoRa.setSpreadingFactor(SPREADING);
  LoRa.setSignalBandwidth(BANDWIDTH);
  LoRa.setTxPower(TX_POWER);
  LoRa.enableCrc();
  Serial.println("[Radio] reset OK, listening.");
}

// =====================================================================
//  Send ACK
// =====================================================================
void sendAck(uint8_t seq, uint8_t deviceIdx, bool actualOn, bool ok) {
  AckPayload ack;
  ack.deviceIndex = deviceIdx;
  ack.actualOn    = actualOn ? 1 : 0;
  ack.ok          = ok ? 1 : 0;

  uint8_t buf[FRAME_MAX];
  uint8_t len = buildFrame(buf, ADDR_GATEWAY, ADDR_NODE1,
                           MSG_ACK, seq, &ack, sizeof(ack));
  loRaSend(buf, len);

  Serial.print("[LoRa] TX ACK seq="); Serial.print(seq);
  Serial.print(" dev="); Serial.print(deviceIdx);
  Serial.print(" actual="); Serial.print(actualOn);
  Serial.print(" ok="); Serial.println(ok);
}

// =====================================================================
//  Send telemetry
// =====================================================================
void sendTelemetry() {
  int16_t tempX10 = 240 + (int16_t)(15.0 * sin(millis() / 20000.0));

  TelemetryPayload t;
  t.temperatureX10 = tempX10;
  t.relayStates    = relayStateBits();

  uint8_t buf[FRAME_MAX];
  uint8_t len = buildFrame(buf, ADDR_GATEWAY, ADDR_NODE1,
                           MSG_TELEMETRY, 0, &t, sizeof(t));
  loRaSend(buf, len);
  Serial.println("[LoRa] TX telemetry");
}

// =====================================================================
//  Send heartbeat
// =====================================================================
void sendHeartbeat() {
  uint8_t buf[FRAME_MAX];
  uint8_t len = buildFrame(buf, ADDR_GATEWAY, ADDR_NODE1,
                           MSG_HEARTBEAT, 0, nullptr, 0);
  loRaSend(buf, len);
  Serial.println("[LoRa] TX heartbeat");
}

// =====================================================================
//  Handle any received message from the gateway
// =====================================================================
void handleIncoming(const uint8_t *buf, uint8_t len) {
  FrameHeader *hdr = (FrameHeader *)buf;

  // Any valid message from the gateway resets the fail-safe timer
  if (hdr->src == ADDR_GATEWAY) {
    lastGatewayHeard = millis();
    failsafeActive = false;
  }

  if (hdr->type == MSG_KEEPALIVE) {
    Serial.println("[LoRa] RX keepalive from gateway");
    return;
  }

  if (hdr->type != MSG_CMD_SET) return;
  if (len < HEADER_SIZE + sizeof(CmdSetPayload) + CRC_SIZE) return;

  CmdSetPayload *cmd = (CmdSetPayload *)(buf + HEADER_SIZE);

  Serial.print("[LoRa] RX CMD seq="); Serial.print(hdr->seq);
  Serial.print(" dev="); Serial.print(cmd->deviceIndex);
  Serial.print(" on="); Serial.println(cmd->requestedOn);

  if (cmd->deviceIndex >= RELAY_COUNT) {
    sendAck(hdr->seq, cmd->deviceIndex, false, false);
    Serial.println("  -> rejected: bad device index");
    return;
  }

  bool wantOn = (cmd->requestedOn == 1);
  setRelay(cmd->deviceIndex, wantOn);
  digitalWrite(PIN_LED, relayStateBits() ? HIGH : LOW);

  sendAck(hdr->seq, cmd->deviceIndex, relayState[cmd->deviceIndex], true);
}

// =====================================================================
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== LoRa node: relay controller (Mega) ===");

  // Relays: safe state first
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    digitalWrite(RELAY_PIN[i], RELAY_ACTIVE_LOW ? HIGH : LOW);
    pinMode(RELAY_PIN[i], OUTPUT);
  }
  pinMode(PIN_LED, OUTPUT);
  Serial.println("Relays initialised (all OFF).");

  // LoRa radio — full hard reset to start clean
  hardResetRadio();

  lastGatewayHeard   = millis();
  lastTelemetry      = millis();
  lastHeartbeat      = millis();
  lastPacketReceived = millis();

  wdt_enable(WDTO_8S);
  Serial.println("Watchdog enabled (8s). Ready.");
}

void loop() {
  wdt_reset();

  // --- Check for incoming LoRa packets ---
  int n = LoRa.parsePacket();
  if (n >= (int)(HEADER_SIZE + CRC_SIZE) && n <= FRAME_MAX) {
    uint8_t buf[FRAME_MAX];
    for (int i = 0; i < n; i++) buf[i] = LoRa.read();

    lastPacketReceived = millis();

    if (validateFrame(buf, n)) {
      FrameHeader *hdr = (FrameHeader *)buf;
      if (hdr->dst == ADDR_NODE1 || hdr->dst == ADDR_BROADCAST) {
        handleIncoming(buf, n);
      }
    } else {
      Serial.println("[LoRa] RX bad CRC, dropped");
    }
  }

  // --- Periodic telemetry (every 10s) ---
  if (millis() - lastTelemetry >= TELEMETRY_INTERVAL_MS) {
    sendTelemetry();
    lastTelemetry = millis();
  }

  // --- Periodic heartbeat (every 30s) ---
  if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }

  // --- Radio watchdog: hard reset if deaf for 15s ---
  if (millis() - lastPacketReceived > RADIO_WATCHDOG_MS) {
    hardResetRadio();
    lastPacketReceived = millis();
  }

  // --- Fail-safe ---
  if (!failsafeActive && millis() - lastGatewayHeard > FAILSAFE_TIMEOUT_MS) {
    Serial.println("[SAFETY] No gateway for 60s -> all relays OFF");
    allRelaysOff();
    digitalWrite(PIN_LED, LOW);
    failsafeActive = true;
  }
}
