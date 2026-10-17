// =====================================================================
//  LoRa Local-First Control Platform - GATEWAY (ESP32)
//  Phase 4: Wi-Fi AP + dashboard + real LoRa commands to the Mega
//
//  What this sketch does:
//   1. Creates the "HomeControl" Wi-Fi network (access point).
//   2. Serves the dashboard at http://192.168.4.1
//   3. When you tap a button on the site, it queues a LoRa command.
//   4. The LoRa task sends the command and waits for the Mega's ACK.
//   5. If no ACK arrives, it retries up to 3 times, then marks FAILED.
//   6. The site polls /api/state and shows the confirmed result.
//
//  The simulated node is gone. This talks to the real Mega.
// =====================================================================

#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <LoRa.h>
#include "secrets.h"
#include "dashboard.h"
#include "device.h"
#include "../common/protocol.h"

WebServer server(80);

// --- LoRa pins (same as the test sketch) ---
const int PIN_SCK  = 18;
const int PIN_MISO = 19;
const int PIN_MOSI = 23;
const int PIN_NSS  = 5;
const int PIN_RST  = 14;
const int PIN_DIO0 = 26;

const long FREQUENCY = 433E6;
const int  SPREADING = 9;
const long BANDWIDTH = 125E3;
const int  TX_POWER  = 10;

// --- Command queue and retry settings ---
const uint32_t ACK_TIMEOUT_MS  = 1500;   // wait this long for a reply
const uint8_t  MAX_RETRIES     = 3;      // try this many times total
const uint32_t RETRY_DELAY_MS  = 200;    // pause between retries

// --- Devices (matches the 4-channel relay on the Mega) ---
Device devices[] = {
  { "light1", "Living room light", false, false, CMD_IDLE, 0 },
  { "light2", "Bedroom light",     false, false, CMD_IDLE, 0 },
  { "fan1",   "Ceiling fan",       false, false, CMD_IDLE, 0 },
  { "pump1",  "Water pump",        false, false, CMD_IDLE, 0 },
};
const int DEVICE_COUNT = sizeof(devices) / sizeof(devices[0]);

Device *findDevice(const String &id) {
  for (int i = 0; i < DEVICE_COUNT; i++)
    if (id == devices[i].id) return &devices[i];
  return nullptr;
}

int deviceIndex(Device *d) {
  return (int)(d - devices);
}

// --- Sequence number (wraps 0-255) ---
uint8_t nextSeq = 0;

// --- Node status ---
bool nodeOnline = false;
uint32_t lastNodeHeard = 0;
const uint32_t NODE_TIMEOUT_MS = 45000;  // mark offline after 45 s silence (3x telemetry interval)

// --- Keepalive: tell the node the gateway is still here ---
const uint32_t KEEPALIVE_INTERVAL_MS = 20000;  // every 20 seconds
uint32_t lastKeepalive = 0;

// --- Telemetry from node ---
float nodeTemperature = 0;

// =====================================================================
//  LoRa send + wait for ACK
// =====================================================================
bool sendAndWaitAck(uint8_t deviceIdx, bool requestedOn, uint8_t seq) {
  // Build the command frame
  CmdSetPayload cmd;
  cmd.deviceIndex = deviceIdx;
  cmd.requestedOn = requestedOn ? 1 : 0;

  uint8_t buf[FRAME_MAX];
  uint8_t len = buildFrame(buf, ADDR_NODE1, ADDR_GATEWAY,
                           MSG_CMD_SET, seq, &cmd, sizeof(cmd));

  for (uint8_t attempt = 0; attempt < MAX_RETRIES; attempt++) {
    if (attempt > 0) delay(RETRY_DELAY_MS);

    // Send
    LoRa.beginPacket();
    LoRa.write(buf, len);
    LoRa.endPacket();
    Serial.print("[LoRa] TX cmd seq="); Serial.print(seq);
    Serial.print(" dev="); Serial.print(deviceIdx);
    Serial.print(" on="); Serial.print(requestedOn);
    Serial.print(" attempt="); Serial.println(attempt + 1);

    // Wait for matching ACK
    uint32_t start = millis();
    while (millis() - start < ACK_TIMEOUT_MS) {
      int n = LoRa.parsePacket();
      if (n < (int)(HEADER_SIZE + CRC_SIZE)) continue;
      if (n > FRAME_MAX) continue;

      uint8_t rxBuf[FRAME_MAX];
      for (int i = 0; i < n; i++) rxBuf[i] = LoRa.read();

      if (!validateFrame(rxBuf, n)) {
        Serial.println("[LoRa] RX bad CRC, dropped");
        continue;
      }

      FrameHeader *hdr = (FrameHeader *)rxBuf;
      if (hdr->dst != ADDR_GATEWAY) continue;   // not for us
      if (hdr->src != ADDR_NODE1)   continue;    // not from our node

      if (hdr->type == MSG_ACK && hdr->seq == seq) {
        AckPayload *ack = (AckPayload *)(rxBuf + HEADER_SIZE);
        Serial.print("[LoRa] ACK dev="); Serial.print(ack->deviceIndex);
        Serial.print(" actual="); Serial.print(ack->actualOn);
        Serial.print(" ok="); Serial.println(ack->ok);

        nodeOnline = true;
        lastNodeHeard = millis();

        if (ack->deviceIndex < DEVICE_COUNT) {
          devices[ack->deviceIndex].confirmedOn = (ack->actualOn == 1);
          devices[ack->deviceIndex].status = CMD_CONFIRMED;
        }
        return true;
      }

      // Handle telemetry or heartbeat that arrives while we wait
      if (hdr->type == MSG_TELEMETRY) {
        handleTelemetry(rxBuf, n);
      } else if (hdr->type == MSG_HEARTBEAT) {
        nodeOnline = true;
        lastNodeHeard = millis();
      }
    }
    Serial.println("[LoRa] timeout, retrying...");
  }
  return false;  // all retries failed
}

// =====================================================================
//  Handle incoming telemetry
// =====================================================================
void handleTelemetry(const uint8_t *buf, uint8_t len) {
  if (len < HEADER_SIZE + sizeof(TelemetryPayload) + CRC_SIZE) return;
  TelemetryPayload *t = (TelemetryPayload *)(buf + HEADER_SIZE);
  nodeTemperature = t->temperatureX10 / 10.0;

  // Update relay states from the node's report
  for (int i = 0; i < DEVICE_COUNT && i < 8; i++) {
    devices[i].confirmedOn = (t->relayStates >> i) & 1;
  }

  nodeOnline = true;
  lastNodeHeard = millis();
  Serial.print("[LoRa] Telemetry: temp="); Serial.print(nodeTemperature);
  Serial.print(" relays=0b"); Serial.println(t->relayStates, BIN);
}

// =====================================================================
//  Background: check for incoming LoRa messages (telemetry, heartbeat)
// =====================================================================
void checkIncoming() {
  int n = LoRa.parsePacket();
  if (n < (int)(HEADER_SIZE + CRC_SIZE) || n > FRAME_MAX) return;

  uint8_t buf[FRAME_MAX];
  for (int i = 0; i < n; i++) buf[i] = LoRa.read();
  if (!validateFrame(buf, n)) return;

  FrameHeader *hdr = (FrameHeader *)buf;
  if (hdr->dst != ADDR_GATEWAY && hdr->dst != ADDR_BROADCAST) return;

  if (hdr->type == MSG_TELEMETRY) {
    handleTelemetry(buf, n);
  } else if (hdr->type == MSG_HEARTBEAT) {
    nodeOnline = true;
    lastNodeHeard = millis();
    Serial.println("[LoRa] Heartbeat received");
  }
}

// =====================================================================
//  Send keepalive to the node so it doesn't trigger its fail-safe
// =====================================================================
void sendKeepalive() {
  uint8_t buf[FRAME_MAX];
  uint8_t len = buildFrame(buf, ADDR_NODE1, ADDR_GATEWAY,
                           MSG_KEEPALIVE, 0, nullptr, 0);
  LoRa.beginPacket();
  LoRa.write(buf, len);
  LoRa.endPacket();
  Serial.println("[LoRa] TX keepalive");
}

// =====================================================================
//  Process pending commands (called from loop)
// =====================================================================
void processPendingCommands() {
  for (int i = 0; i < DEVICE_COUNT; i++) {
    Device &d = devices[i];
    if (d.status != CMD_PENDING) continue;

    bool ok = sendAndWaitAck(i, d.requestedOn, d.seq);
    if (!ok) {
      d.status = CMD_FAILED;
      Serial.print("[LoRa] FAILED dev="); Serial.println(i);
    }
    return;  // one command per loop pass, so the web server stays responsive
  }
}

// =====================================================================
//  HTTP handlers (same API as before)
// =====================================================================
void handleRoot() {
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

void handleState() {
  // Check if node has gone silent
  if (nodeOnline && millis() - lastNodeHeard > NODE_TIMEOUT_MS) {
    nodeOnline = false;
  }

  String json = "{";
  json += "\"uptime_s\":" + String(millis() / 1000) + ",";
  json += "\"clients\":" + String(WiFi.softAPgetStationNum()) + ",";
  json += "\"node_online\":" + String(nodeOnline ? "true" : "false") + ",";
  json += "\"temperature_c\":" + String(nodeTemperature, 1) + ",";
  json += "\"rssi\":" + String(LoRa.packetRssi()) + ",";
  json += "\"devices\":[";
  for (int i = 0; i < DEVICE_COUNT; i++) {
    Device &d = devices[i];
    if (i) json += ",";
    json += "{\"id\":\"" + String(d.id) + "\",";
    json += "\"name\":\"" + String(d.name) + "\",";
    json += "\"on\":" + String(d.confirmedOn ? "true" : "false") + ",";
    json += "\"status\":\"" + String(statusName(d.status)) + "\"}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleSetDevice() {
  if (!server.hasArg("id") || !server.hasArg("state")) {
    server.send(400, "application/json", "{\"error\":\"need id and state\"}");
    return;
  }
  Device *d = findDevice(server.arg("id"));
  if (!d) {
    server.send(404, "application/json", "{\"error\":\"unknown device\"}");
    return;
  }
  String state = server.arg("state");
  if (state != "on" && state != "off") {
    server.send(400, "application/json", "{\"error\":\"state must be on or off\"}");
    return;
  }
  if (d->status == CMD_PENDING) {
    server.send(409, "application/json", "{\"error\":\"command already pending\"}");
    return;
  }

  d->requestedOn = (state == "on");
  d->status      = CMD_PENDING;
  d->sentAt      = millis();
  d->seq         = nextSeq++;

  server.send(202, "application/json", "{\"status\":\"PENDING\"}");
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found");
}

// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== LoRa gateway: AP + dashboard + real LoRa ===");

  // Wi-Fi access point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSPHRASE);
  Serial.print("Wi-Fi: "); Serial.println(AP_SSID);
  Serial.print("URL:   http://"); Serial.println(WiFi.softAPIP());

  // LoRa radio
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_NSS);
  LoRa.setPins(PIN_NSS, PIN_RST, PIN_DIO0);
  if (!LoRa.begin(FREQUENCY)) {
    Serial.println("FAIL: LoRa module not found.");
    while (true) delay(1000);
  }
  LoRa.setSpreadingFactor(SPREADING);
  LoRa.setSignalBandwidth(BANDWIDTH);
  LoRa.setTxPower(TX_POWER);
  LoRa.enableCrc();
  Serial.println("LoRa radio started at 433 MHz.");

  // Web server
  server.on("/",            HTTP_GET,  handleRoot);
  server.on("/api/state",   HTTP_GET,  handleState);
  server.on("/api/devices", HTTP_POST, handleSetDevice);
  server.onNotFound(handleNotFound);
  server.begin();
  lastKeepalive = millis();
  Serial.println("Web server started. Ready.");
}

void loop() {
  server.handleClient();
  processPendingCommands();
  checkIncoming();

  // Send keepalive to the node every 20 seconds
  if (millis() - lastKeepalive >= KEEPALIVE_INTERVAL_MS) {
    sendKeepalive();
    lastKeepalive = millis();
  }
}
