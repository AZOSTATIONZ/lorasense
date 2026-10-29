// =====================================================================
//  LoRa Local-First Control Platform - NODE (Arduino Mega 2560)
//  Phase 4: protocol v2, DHT11 sensor, relay control + ACK + telemetry
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
#include <DHT.h>
#include "protocol.h"

// --- Relay pins (supports 1/2/4/8-channel modules) ---
const uint8_t RELAY_PIN[] = { 22, 23, 24, 25, 26, 27, 28, 29 };
const uint8_t RELAY_COUNT = 4;  // active channel count — change to match your relay module
const bool RELAY_ACTIVE_LOW = false;  // this module: HIGH = relay ON

// --- Relay state ---
bool relayState[8] = { false };

// --- LoRa pins ---
const int PIN_NSS  = 53;
const int PIN_RST  = 9;
const int PIN_DIO0 = 2;

const long FREQUENCY = 433E6;
const int  SPREADING = 9;
const long BANDWIDTH = 125E3;
const int  TX_POWER  = 10;

// --- Timing (mutable for config sync) ---
uint32_t telemetryIntervalMs = 10000;
uint32_t heartbeatIntervalMs = 30000;
uint32_t failsafeTimeoutMs   = 60000;
const uint32_t RADIO_WATCHDOG_MS     = 15000;

uint32_t lastTelemetry = 0;
uint32_t lastHeartbeat = 0;
uint32_t lastGatewayHeard = 0;
uint32_t lastPacketReceived = 0;
bool failsafeActive = false;

const int PIN_LED = 13;

// --- DHT sensor (dynamically re-initializable for config sync) ---
uint8_t configDhtPin = 7;
DHT *dhtSensor = nullptr;
float lastTempC = 0;
float lastHumid = 0;
bool  sensorOk  = false;
uint8_t configAnalogPin = 0xFF;  // 0xFF = no analog sensor
uint8_t configVersion = 0xFF;    // 0xFF = no config received yet

// --- Node identity (change for each node: ADDR_NODE1=0x02, ADDR_NODE2=0x03, etc.) ---
const uint8_t MY_ADDRESS = ADDR_NODE1;

void initDht(uint8_t pin) {
  if (dhtSensor) delete dhtSensor;
  configDhtPin = pin;
  dhtSensor = new DHT(pin, DHT22);
  dhtSensor->begin();
  Serial.print("[Config] DHT sensor on pin "); Serial.println(pin);
}

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
  uint8_t len = buildFrame(buf, ADDR_GATEWAY, MY_ADDRESS,
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
void readSensor() {
  if (!dhtSensor) return;
  float t = dhtSensor->readTemperature();
  float h = dhtSensor->readHumidity();
  if (isnan(t) || isnan(h)) {
    Serial.println("[DHT] read FAILED (NaN)");
    return;
  }
  lastTempC = t;
  lastHumid = h;
  sensorOk  = true;
  Serial.print("[DHT] temp="); Serial.print(t);
  Serial.print(" hum="); Serial.println(h);
}

void sendTelemetry() {
  readSensor();

  TelemetryPayload t;
  t.temperatureX10 = (int16_t)(lastTempC * 10);
  t.humidityX10    = (int16_t)(lastHumid * 10);
  t.relayStates    = relayStateBits();

  if (configAnalogPin != 0xFF) {
    t.analogValue = (int16_t)analogRead(configAnalogPin);
    t.analogPin   = configAnalogPin;
  } else {
    t.analogValue = -1;
    t.analogPin   = 0xFF;
  }

  uint8_t buf[FRAME_MAX];
  uint8_t len = buildFrame(buf, ADDR_GATEWAY, MY_ADDRESS,
                           MSG_TELEMETRY, 0, &t, sizeof(t));
  loRaSend(buf, len);
  Serial.println("[LoRa] TX telemetry");
}

// =====================================================================
//  Send heartbeat
// =====================================================================
void sendHeartbeat() {
  uint8_t buf[FRAME_MAX];
  uint8_t len = buildFrame(buf, ADDR_GATEWAY, MY_ADDRESS,
                           MSG_HEARTBEAT, 0, nullptr, 0);
  loRaSend(buf, len);
  Serial.println("[LoRa] TX heartbeat");
}

// =====================================================================
//  Handle any received message from the gateway
// =====================================================================
void handleIncoming(const uint8_t *buf, uint8_t len) {
  FrameHeader *hdr = (FrameHeader *)buf;

  if (hdr->ver != PROTOCOL_VERSION) {
    Serial.print("[LoRa] version mismatch: got v");
    Serial.print(hdr->ver);
    Serial.print(" expected v");
    Serial.println(PROTOCOL_VERSION);
    return;
  }

  // Any valid message from the gateway resets the fail-safe timer
  if (hdr->src == ADDR_GATEWAY) {
    lastGatewayHeard = millis();
    failsafeActive = false;
  }

  if (hdr->type == MSG_KEEPALIVE) {
    Serial.println("[LoRa] RX keepalive from gateway");
    return;
  }

  if (hdr->type == MSG_CONFIG_PUSH) {
    if (len < HEADER_SIZE + sizeof(ConfigPushPayload) + CRC_SIZE) return;
    ConfigPushPayload *cfg = (ConfigPushPayload *)(buf + HEADER_SIZE);

    if (cfg->configType == CFG_FULL_SYNC && cfg->dataLen == 8) {
      uint8_t ver       = cfg->data[0];
      uint8_t sensType  = cfg->data[1];
      uint8_t sensPin   = cfg->data[2];
      uint8_t aPin      = cfg->data[3];
      uint16_t telSec   = ((uint16_t)cfg->data[4] << 8) | cfg->data[5];
      uint16_t fsSec    = ((uint16_t)cfg->data[6] << 8) | cfg->data[7];

      Serial.print("[Config] v"); Serial.print(ver);
      Serial.print(" dht="); Serial.print(sensType);
      Serial.print("/pin"); Serial.print(sensPin);
      Serial.print(" adc=A"); Serial.print(aPin != 0xFF ? aPin - 54 : -1);
      Serial.print(" tel="); Serial.print(telSec);
      Serial.print("s fs="); Serial.print(fsSec);
      Serial.println("s");

      if (sensPin != 0xFF && sensPin != configDhtPin) {
        initDht(sensPin);
      }
      configAnalogPin = aPin;
      if (telSec >= 2 && telSec <= 300) {
        telemetryIntervalMs = (uint32_t)telSec * 1000;
      }
      if (fsSec >= 10 && fsSec <= 600) {
        failsafeTimeoutMs = (uint32_t)fsSec * 1000;
      }
      configVersion = ver;

      // Send CONFIG_ACK
      ConfigAckPayload ack;
      ack.configType = CFG_FULL_SYNC;
      ack.version    = ver;
      uint8_t ackBuf[FRAME_MAX];
      uint8_t ackLen = buildFrame(ackBuf, ADDR_GATEWAY, MY_ADDRESS,
                                  MSG_CONFIG_ACK, hdr->seq, &ack, sizeof(ack));
      loRaSend(ackBuf, ackLen);
      Serial.print("[Config] ACK sent for v"); Serial.println(ver);
    }
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
  Serial.println("=== LoRaSense Node v3.0.0 (Mega) ===");

  // Relays: safe state first
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    digitalWrite(RELAY_PIN[i], RELAY_ACTIVE_LOW ? HIGH : LOW);
    pinMode(RELAY_PIN[i], OUTPUT);
  }
  pinMode(PIN_LED, OUTPUT);
  Serial.println("Relays initialised (all OFF).");

  initDht(configDhtPin);
  delay(2000);

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
      if (hdr->dst == MY_ADDRESS || hdr->dst == ADDR_BROADCAST) {
        handleIncoming(buf, n);
      }
    } else {
      Serial.println("[LoRa] RX bad CRC, dropped");
    }
  }

  // --- Periodic telemetry ---
  if (millis() - lastTelemetry >= telemetryIntervalMs) {
    sendTelemetry();
    lastTelemetry = millis();
  }

  // --- Periodic heartbeat ---
  if (millis() - lastHeartbeat >= heartbeatIntervalMs) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }

  // --- Radio watchdog: hard reset if deaf for 15s ---
  if (millis() - lastPacketReceived > RADIO_WATCHDOG_MS) {
    hardResetRadio();
    lastPacketReceived = millis();
  }

  // --- Fail-safe ---
  if (!failsafeActive && millis() - lastGatewayHeard > failsafeTimeoutMs) {
    Serial.println("[SAFETY] No gateway for 60s -> all relays OFF");
    allRelaysOff();
    digitalWrite(PIN_LED, LOW);
    failsafeActive = true;
  }
}
