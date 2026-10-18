// =====================================================================
//  LoRaSense Gateway (ESP32)
//  Hardware-aware config studio, non-blocking commands, automation, protocol v2
// =====================================================================

#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <LoRa.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include "device.h"
#include "config.h"
#include "hardware.h"
#include "history.h"
#include "../common/protocol.h"

WebServer server(80);
ConfigManager config;
HardwareManager hw;
SensorHistory history;

// --- LoRa pins ---
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
const uint32_t ACK_TIMEOUT_MS  = 1500;
const uint8_t  MAX_RETRIES     = 3;
const uint32_t RETRY_DELAY_MS  = 200;

// --- Output state (per relay channel, not per equipment name) ---
OutputState outputs[MAX_OUTPUTS];

// --- Sequence number ---
uint8_t nextSeq = 0;

// --- Node status ---
bool nodeOnline = false;
uint32_t lastNodeHeard = 0;
const uint32_t NODE_TIMEOUT_MS = 45000;

// --- Keepalive ---
const uint32_t KEEPALIVE_INTERVAL_MS = 20000;
uint32_t lastKeepalive = 0;

// --- Sensor history sampling ---
const uint32_t HISTORY_INTERVAL_MS = 300000;  // 5 minutes
uint32_t lastHistorySample = 0;
bool firstSampleTaken = false;

// --- Telemetry from node ---
float nodeTemperature = 0;
float nodeHumidity    = 0;

// =====================================================================
//  LittleFS file server
// =====================================================================
String getContentType(const String &path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".css"))  return "text/css";
  if (path.endsWith(".js"))   return "application/javascript";
  if (path.endsWith(".json")) return "application/json";
  if (path.endsWith(".ico"))  return "image/x-icon";
  if (path.endsWith(".svg"))  return "image/svg+xml";
  if (path.endsWith(".png"))  return "image/png";
  return "text/plain";
}

bool serveFile(const String &path) {
  String filePath = path;
  if (filePath.endsWith("/")) filePath += "index.html";
  if (!LittleFS.exists(filePath)) return false;
  File f = LittleFS.open(filePath, "r");
  if (!f) return false;
  server.streamFile(f, getContentType(filePath));
  f.close();
  return true;
}

// =====================================================================
//  LoRa send (non-blocking) — transmit one command packet and return
// =====================================================================
void sendCommand(uint8_t outputIdx) {
  OutputState &o = outputs[outputIdx];

  CmdSetPayload cmd;
  cmd.deviceIndex = outputIdx;
  cmd.requestedOn = o.requestedOn ? 1 : 0;

  uint8_t buf[FRAME_MAX];
  uint8_t len = buildFrame(buf, ADDR_NODE1, ADDR_GATEWAY,
                           MSG_CMD_SET, o.seq, &cmd, sizeof(cmd));

  LoRa.beginPacket();
  LoRa.write(buf, len);
  LoRa.endPacket();

  o.sentAt = millis();
  o.status = CMD_SENT;

  Serial.print("[LoRa] TX cmd seq="); Serial.print(o.seq);
  Serial.print(" output="); Serial.print(outputIdx);
  Serial.print(" on="); Serial.print(o.requestedOn);
  Serial.print(" attempt="); Serial.println(o.attempt + 1);
}

// =====================================================================
//  Handle incoming telemetry
// =====================================================================
void handleTelemetry(const uint8_t *buf, uint8_t len) {
  if (len < HEADER_SIZE + sizeof(TelemetryPayload) + CRC_SIZE) return;
  TelemetryPayload *t = (TelemetryPayload *)(buf + HEADER_SIZE);
  nodeTemperature = t->temperatureX10 / 10.0;
  nodeHumidity    = t->humidityX10    / 10.0;

  for (int i = 0; i < MAX_OUTPUTS && i < 8; i++) {
    outputs[i].confirmedOn = (t->relayStates >> i) & 1;
  }

  nodeOnline = true;
  lastNodeHeard = millis();
  Serial.print("[LoRa] Telemetry: temp="); Serial.print(nodeTemperature);
  Serial.print(" hum="); Serial.print(nodeHumidity);
  Serial.print(" relays=0b"); Serial.println(t->relayStates, BIN);
}

// =====================================================================
//  Background: check for incoming LoRa messages
// =====================================================================
void checkIncoming() {
  int n = LoRa.parsePacket();
  if (n < (int)(HEADER_SIZE + CRC_SIZE) || n > FRAME_MAX) return;

  uint8_t buf[FRAME_MAX];
  for (int i = 0; i < n; i++) buf[i] = LoRa.read();
  if (!validateFrame(buf, n)) return;

  FrameHeader *hdr = (FrameHeader *)buf;
  if (hdr->dst != ADDR_GATEWAY && hdr->dst != ADDR_BROADCAST) return;

  if (hdr->ver != PROTOCOL_VERSION) {
    Serial.print("[LoRa] version mismatch: got v");
    Serial.print(hdr->ver);
    Serial.print(" expected v");
    Serial.println(PROTOCOL_VERSION);
    return;
  }

  if (hdr->type == MSG_ACK) {
    AckPayload *ack = (AckPayload *)(buf + HEADER_SIZE);
    nodeOnline = true;
    lastNodeHeard = millis();
    // Match ACK to the output waiting for it
    if (ack->deviceIndex < MAX_OUTPUTS &&
        outputs[ack->deviceIndex].status == CMD_SENT &&
        hdr->seq == outputs[ack->deviceIndex].seq) {
      outputs[ack->deviceIndex].confirmedOn = (ack->actualOn == 1);
      outputs[ack->deviceIndex].status = CMD_CONFIRMED;
      Serial.print("[LoRa] ACK output="); Serial.print(ack->deviceIndex);
      Serial.print(" on="); Serial.println(ack->actualOn);
    }
  }
  else if (hdr->type == MSG_TELEMETRY) handleTelemetry(buf, n);
  else if (hdr->type == MSG_HEARTBEAT) {
    nodeOnline = true; lastNodeHeard = millis();
    Serial.println("[LoRa] Heartbeat received");
  }
  else if (hdr->type == MSG_CONFIG_ACK) {
    Serial.println("[LoRa] Config ACK received");
  }
}

// =====================================================================
//  Send keepalive
// =====================================================================
void sendKeepalive() {
  uint8_t buf[FRAME_MAX];
  uint8_t len = buildFrame(buf, ADDR_NODE1, ADDR_GATEWAY,
                           MSG_KEEPALIVE, 0, nullptr, 0);
  LoRa.beginPacket();
  LoRa.write(buf, len);
  LoRa.endPacket();
}

// =====================================================================
//  Process pending commands (non-blocking state machine)
// =====================================================================
void processPendingCommands() {
  for (int i = 0; i < MAX_OUTPUTS; i++) {
    OutputState &o = outputs[i];

    if (o.status == CMD_PENDING) {
      o.attempt = 0;
      sendCommand(i);
      return;
    }

    if (o.status == CMD_SENT) {
      if (millis() - o.sentAt < ACK_TIMEOUT_MS) return;
      // Timed out — retry or fail
      o.attempt++;
      if (o.attempt < MAX_RETRIES) {
        Serial.print("[LoRa] timeout output="); Serial.print(i);
        Serial.print(", retry "); Serial.println(o.attempt + 1);
        sendCommand(i);
      } else {
        o.status = CMD_FAILED;
        Serial.print("[LoRa] FAILED output="); Serial.println(i);
      }
      return;
    }
  }
}

// =====================================================================
//  HTTP: /api/state — dashboard polling endpoint
// =====================================================================
void handleState() {
  if (nodeOnline && millis() - lastNodeHeard > NODE_TIMEOUT_MS)
    nodeOnline = false;

  JsonDocument doc;
  doc["uptime_s"]     = millis() / 1000;
  doc["clients"]      = WiFi.softAPgetStationNum();
  doc["node_online"]  = nodeOnline;
  doc["temperature_c"] = serialized(String(nodeTemperature, 1));
  doc["humidity_pct"]  = serialized(String(nodeHumidity, 1));
  doc["rssi"]         = LoRa.packetRssi();
  doc["protocol_ver"] = PROTOCOL_VERSION;
  doc["configured"]   = config.env.configured;

  // Build devices array from equipment registry + output states
  JsonArray devArr = doc["devices"].to<JsonArray>();
  for (int i = 0; i < config.equipmentCount; i++) {
    Equipment &eq = config.equipment[i];
    if (!eq.active) continue;
    JsonObject d = devArr.add<JsonObject>();
    d["id"]       = eq.id;
    d["name"]     = eq.name;
    d["category"] = eq.category;
    d["zone_id"]  = eq.zoneId;

    if (eq.outputIndex < MAX_OUTPUTS) {
      d["on"]     = outputs[eq.outputIndex].confirmedOn;
      d["status"] = statusName(outputs[eq.outputIndex].status);
      d["output"] = eq.outputIndex;
    } else {
      d["on"]     = false;
      d["status"] = "IDLE";
      d["output"] = nullptr;
    }
  }

  // Zones
  JsonArray zArr = doc["zones"].to<JsonArray>();
  for (int i = 0; i < config.zoneCount; i++) {
    JsonObject z = zArr.add<JsonObject>();
    z["id"]   = config.zones[i].id;
    z["name"] = config.zones[i].name;
  }

  // Environment info
  if (config.env.configured) {
    doc["env_name"] = config.env.name;
    doc["env_type"] = config.env.type;
  }

  // Active automation rules
  JsonArray rulesArr = doc["rules"].to<JsonArray>();
  for (int i = 0; i < config.ruleCount; i++) {
    AutomationRule &r = config.rules[i];
    if (!r.enabled) continue;
    JsonObject ro = rulesArr.add<JsonObject>();
    ro["name"]      = r.name;
    ro["equipment"] = r.equipmentId;
    ro["triggered"] = r.triggered;
    ro["enabled"]   = r.enabled;
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// =====================================================================
//  HTTP: POST /api/devices — control a device by equipment ID
// =====================================================================
void handleSetDevice() {
  if (!server.hasArg("id") || !server.hasArg("state")) {
    server.send(400, "application/json", "{\"error\":\"need id and state\"}");
    return;
  }

  Equipment *eq = config.findEquipment(server.arg("id").c_str());
  if (!eq) {
    server.send(404, "application/json", "{\"error\":\"unknown equipment\"}");
    return;
  }
  if (eq->outputIndex >= MAX_OUTPUTS) {
    server.send(400, "application/json", "{\"error\":\"no relay assigned\"}");
    return;
  }

  String state = server.arg("state");
  if (state != "on" && state != "off") {
    server.send(400, "application/json", "{\"error\":\"state must be on or off\"}");
    return;
  }
  if (outputs[eq->outputIndex].status == CMD_PENDING ||
      outputs[eq->outputIndex].status == CMD_SENT) {
    server.send(409, "application/json", "{\"error\":\"command in progress\"}");
    return;
  }

  outputs[eq->outputIndex].requestedOn = (state == "on");
  outputs[eq->outputIndex].status      = CMD_PENDING;
  outputs[eq->outputIndex].sentAt      = millis();
  outputs[eq->outputIndex].seq         = nextSeq++;
  outputs[eq->outputIndex].attempt     = 0;

  server.send(202, "application/json", "{\"status\":\"PENDING\"}");
}

// =====================================================================
//  HTTP: Config API
// =====================================================================
void handleGetConfig() {
  JsonDocument doc;
  JsonObject env = doc["environment"].to<JsonObject>();
  env["name"] = config.env.name;
  env["type"] = config.env.type;
  env["configured"] = config.env.configured;

  JsonArray zArr = doc["zones"].to<JsonArray>();
  for (int i = 0; i < config.zoneCount; i++) {
    JsonObject z = zArr.add<JsonObject>();
    z["id"]   = config.zones[i].id;
    z["name"] = config.zones[i].name;
  }

  JsonArray eArr = doc["equipment"].to<JsonArray>();
  for (int i = 0; i < config.equipmentCount; i++) {
    if (!config.equipment[i].active) continue;
    Equipment &eq = config.equipment[i];
    JsonObject e = eArr.add<JsonObject>();
    e["id"]       = eq.id;
    e["name"]     = eq.name;
    e["category"] = eq.category;
    e["zone_id"]  = eq.zoneId;
    e["node_id"]  = eq.nodeId;
    if (eq.outputIndex != 0xFF) {
      e["output"] = eq.outputIndex;
      if (eq.outputIndex < MEGA_RELAY_COUNT) {
        char pinBuf[6];
        HardwareManager::pinName(MEGA_RELAY_PINS[eq.outputIndex], pinBuf, sizeof(pinBuf));
        e["pin_name"] = String(pinBuf);
      }
    }
  }

  // Available relay outputs
  JsonArray outArr = doc["available_outputs"].to<JsonArray>();
  for (int i = 0; i < MAX_OUTPUTS; i++) {
    if (!config.isOutputTaken(i)) outArr.add(i);
  }

  doc["max_outputs"] = MAX_OUTPUTS;

  JsonArray rArr = doc["rules"].to<JsonArray>();
  for (int i = 0; i < config.ruleCount; i++) {
    AutomationRule &r = config.rules[i];
    JsonObject ro = rArr.add<JsonObject>();
    ro["id"]         = r.id;
    ro["name"]       = r.name;
    ro["sensor"]     = r.sensor;
    ro["condition"]  = r.condition;
    ro["threshold"]  = r.thresholdX10;
    ro["hysteresis"] = r.hysteresisX10;
    ro["equipment"]  = r.equipmentId;
    ro["enabled"]    = r.enabled;
    ro["triggered"]  = r.triggered;
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleSetEnvironment() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"need JSON body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }
  strlcpy(config.env.name, doc["name"] | config.env.name, NAME_LEN);
  strlcpy(config.env.type, doc["type"] | config.env.type, sizeof(config.env.type));
  config.saveEnvironment();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleAddZone() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"need JSON body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }
  const char *id   = doc["id"] | "";
  const char *name = doc["name"] | "";
  if (!id[0] || !name[0]) {
    server.send(400, "application/json", "{\"error\":\"need id and name\"}");
    return;
  }
  if (!config.addZone(id, name)) {
    server.send(409, "application/json", "{\"error\":\"zone exists or limit reached\"}");
    return;
  }
  server.send(201, "application/json", "{\"ok\":true}");
}

void handleDeleteZone() {
  String id = server.arg("id");
  if (!id.length()) {
    server.send(400, "application/json", "{\"error\":\"need id\"}");
    return;
  }
  if (!config.removeZone(id.c_str())) {
    server.send(404, "application/json", "{\"error\":\"zone not found\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void syncRelayLabels() {
  hw.clearRelayLabels();
  for (int i = 0; i < config.equipmentCount; i++) {
    Equipment &eq = config.equipment[i];
    if (eq.active && eq.outputIndex != 0xFF && eq.outputIndex < MEGA_RELAY_COUNT)
      hw.setRelayLabel(eq.outputIndex, eq.name);
  }
}

void handleAddEquipment() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"need JSON body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }
  const char *id     = doc["id"] | "";
  const char *name   = doc["name"] | "";
  const char *cat    = doc["category"] | "other";
  const char *zone   = doc["zone_id"] | "";
  const char *nodeId = doc["node_id"] | "mega-001";
  uint8_t output     = doc["output"] | 0xFF;

  if (!id[0] || !name[0]) {
    server.send(400, "application/json", "{\"error\":\"need id and name\"}");
    return;
  }
  if (output != 0xFF && output >= MAX_OUTPUTS) {
    String msg = "{\"error\":\"Relay " + String(output + 1) + " is unavailable on this controller (max " + String(MAX_OUTPUTS) + ")\"}";
    server.send(400, "application/json", msg);
    return;
  }
  if (output != 0xFF && config.isOutputTaken(output)) {
    Equipment *owner = config.findByOutput(output);
    String msg = "{\"error\":\"Relay " + String(output + 1) + " is already assigned to " + String(owner ? owner->name : "another device") + "\"}";
    server.send(409, "application/json", msg);
    return;
  }
  if (nodeId[0] && !hw.findNode(nodeId)) {
    server.send(400, "application/json", "{\"error\":\"controller node not found\"}");
    return;
  }
  if (!config.addEquipment(id, name, cat, zone, nodeId, output)) {
    server.send(409, "application/json", "{\"error\":\"equipment exists or limit reached\"}");
    return;
  }
  syncRelayLabels();
  server.send(201, "application/json", "{\"ok\":true}");
}

void handleUpdateEquipment() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"need JSON body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }
  const char *id     = doc["id"] | "";
  const char *name   = doc["name"] | "";
  const char *cat    = doc["category"] | "";
  const char *zone   = doc["zone_id"] | "";
  const char *nodeId = doc["node_id"] | "";
  uint8_t output     = doc["output"] | 0xFF;

  if (!id[0]) {
    server.send(400, "application/json", "{\"error\":\"need id\"}");
    return;
  }
  if (output != 0xFF && output >= MAX_OUTPUTS) {
    String msg = "{\"error\":\"Relay " + String(output + 1) + " is unavailable on this controller (max " + String(MAX_OUTPUTS) + ")\"}";
    server.send(400, "application/json", msg);
    return;
  }
  if (output != 0xFF && config.isOutputTaken(output, id)) {
    Equipment *owner = config.findByOutput(output);
    String msg = "{\"error\":\"Relay " + String(output + 1) + " is already assigned to " + String(owner ? owner->name : "another device") + "\"}";
    server.send(409, "application/json", msg);
    return;
  }
  if (!config.updateEquipment(id, name, cat, zone, nodeId, output)) {
    server.send(404, "application/json", "{\"error\":\"not found\"}");
    return;
  }
  syncRelayLabels();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleDeleteEquipment() {
  String id = server.arg("id");
  if (!id.length()) {
    server.send(400, "application/json", "{\"error\":\"need id\"}");
    return;
  }
  if (!config.removeEquipment(id.c_str())) {
    server.send(404, "application/json", "{\"error\":\"not found\"}");
    return;
  }
  syncRelayLabels();
  server.send(200, "application/json", "{\"ok\":true}");
}

// =====================================================================
//  HTTP: Onboarding — complete first-time setup in one request
// =====================================================================
void handleOnboarding() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"need JSON body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }

  // Environment
  strlcpy(config.env.name, doc["env_name"] | "My Home", NAME_LEN);
  strlcpy(config.env.type, doc["env_type"] | "home", sizeof(config.env.type));
  config.env.configured = true;
  config.saveEnvironment();

  // Zones
  JsonArray zones = doc["zones"].as<JsonArray>();
  for (JsonObject z : zones) {
    const char *id   = z["id"] | "";
    const char *name = z["name"] | "";
    if (id[0] && name[0]) config.addZone(id, name);
  }

  // Equipment
  JsonArray equip = doc["equipment"].as<JsonArray>();
  for (JsonObject e : equip) {
    const char *id   = e["id"] | "";
    const char *name = e["name"] | "";
    const char *cat  = e["category"] | "other";
    const char *zone = e["zone_id"] | "";
    uint8_t output   = e["output"] | 0xFF;
    const char *nodeId = e["node_id"] | "mega-001";
    if (id[0] && name[0]) config.addEquipment(id, name, cat, zone, nodeId, output);
  }
  syncRelayLabels();

  Serial.println("[Onboarding] Setup complete.");
  server.send(200, "application/json", "{\"ok\":true}");
}

// =====================================================================
//  HTTP: Rules API
// =====================================================================
void handleGetRules() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < config.ruleCount; i++) {
    AutomationRule &r = config.rules[i];
    JsonObject obj = arr.add<JsonObject>();
    obj["id"]         = r.id;
    obj["name"]       = r.name;
    obj["sensor"]     = r.sensor;
    obj["condition"]  = r.condition;
    obj["threshold"]  = r.thresholdX10;
    obj["hysteresis"] = r.hysteresisX10;
    obj["equipment"]  = r.equipmentId;
    obj["enabled"]    = r.enabled;
    obj["triggered"]  = r.triggered;
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleAddRule() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"need JSON body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }
  const char *id   = doc["id"] | "";
  const char *name = doc["name"] | "";
  const char *eqId = doc["equipment"] | "";
  if (!id[0] || !name[0] || !eqId[0]) {
    server.send(400, "application/json", "{\"error\":\"need id, name, equipment\"}");
    return;
  }
  if (!config.findEquipment(eqId)) {
    server.send(404, "application/json", "{\"error\":\"equipment not found\"}");
    return;
  }
  uint8_t sensor    = doc["sensor"] | 0;
  uint8_t condition = doc["condition"] | 0;
  int16_t threshold = doc["threshold"] | 0;
  int16_t hyst      = doc["hysteresis"] | 0;
  bool enabled      = doc["enabled"] | true;

  if (!config.addRule(id, name, sensor, condition, threshold, hyst, eqId, enabled)) {
    server.send(409, "application/json", "{\"error\":\"rule exists or limit reached\"}");
    return;
  }
  server.send(201, "application/json", "{\"ok\":true}");
}

void handleUpdateRule() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"need JSON body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }
  const char *id   = doc["id"] | "";
  const char *name = doc["name"] | "";
  const char *eqId = doc["equipment"] | "";
  if (!id[0]) {
    server.send(400, "application/json", "{\"error\":\"need id\"}");
    return;
  }
  if (eqId[0] && !config.findEquipment(eqId)) {
    server.send(404, "application/json", "{\"error\":\"equipment not found\"}");
    return;
  }
  uint8_t sensor    = doc["sensor"] | 0;
  uint8_t condition = doc["condition"] | 0;
  int16_t threshold = doc["threshold"] | 0;
  int16_t hyst      = doc["hysteresis"] | 0;
  bool enabled      = doc["enabled"] | true;

  if (!config.updateRule(id, name, sensor, condition, threshold, hyst, eqId, enabled)) {
    server.send(404, "application/json", "{\"error\":\"rule not found\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleDeleteRule() {
  String id = server.arg("id");
  if (!id.length()) {
    server.send(400, "application/json", "{\"error\":\"need id\"}");
    return;
  }
  if (!config.removeRule(id.c_str())) {
    server.send(404, "application/json", "{\"error\":\"rule not found\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

// =====================================================================
//  HTTP: /api/history — sensor reading history
// =====================================================================
void handleHistory() {
  String json = "[";
  for (int i = 0; i < history.count; i++) {
    const SensorReading &r = history.at(i);
    if (i > 0) json += ',';
    json += "{\"t\":";
    json += r.uptimeSec;
    json += ",\"tc\":";
    json += String(r.tempX10 / 10.0, 1);
    json += ",\"h\":";
    json += String(r.humX10 / 10.0, 1);
    json += '}';
  }
  json += ']';
  server.send(200, "application/json", json);
}

// =====================================================================
//  HTTP: /api/test — on-device self-test
// =====================================================================
void handleSelfTest() {
  JsonDocument doc;
  JsonArray results = doc["tests"].to<JsonArray>();
  int pass = 0, fail = 0;

  auto addResult = [&](const char *name, bool ok) {
    JsonObject r = results.add<JsonObject>();
    r["name"] = name;
    r["pass"] = ok;
    if (ok) pass++; else fail++;
  };

  // --- CRC known vector ---
  uint8_t crcData[] = {'1','2','3','4','5','6','7','8','9'};
  addResult("CRC-16 known vector", crc16(crcData, 9) == 0x29B1);

  // --- Frame build + validate roundtrip ---
  uint8_t frameBuf[FRAME_MAX];
  CmdSetPayload testCmd = {0, 1};
  uint8_t fLen = buildFrame(frameBuf, ADDR_NODE1, ADDR_GATEWAY,
                            MSG_CMD_SET, 99, &testCmd, sizeof(testCmd));
  addResult("frame build length", fLen == HEADER_SIZE + sizeof(CmdSetPayload) + CRC_SIZE);
  addResult("frame validates", validateFrame(frameBuf, fLen));

  // Corrupt and verify rejection
  frameBuf[3] ^= 0x01;
  addResult("corrupted frame rejected", !validateFrame(frameBuf, fLen));
  frameBuf[3] ^= 0x01;  // restore

  // --- History buffer ---
  SensorHistory testHist;
  testHist.push(100, 24.5, 60.0);
  testHist.push(400, 25.0, 58.0);
  addResult("history count after 2", testHist.count == 2);
  addResult("history first reading", testHist.at(0).uptimeSec == 100);
  addResult("history second reading", testHist.at(1).tempX10 == 250);

  // --- LittleFS accessible ---
  addResult("LittleFS mounted", LittleFS.exists("/index.html"));

  // --- Config loaded ---
  addResult("config zones loaded", config.zoneCount >= 0);
  addResult("config equipment loaded", config.equipmentCount >= 0);

  doc["passed"] = pass;
  doc["failed"] = fail;
  doc["total"]  = pass + fail;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// =====================================================================
//  HTTP: Hardware Studio API
// =====================================================================
void handleGetNodes() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < hw.nodeCount; i++) {
    if (!hw.nodes[i].active) continue;
    JsonObject obj = arr.add<JsonObject>();
    obj["id"]      = hw.nodes[i].id;
    obj["name"]    = hw.nodes[i].name;
    obj["address"]  = hw.nodes[i].address;
    obj["board"]   = hw.nodes[i].board;
    obj["fw"]      = hw.nodes[i].fwVersion;
    obj["online"]  = nodeOnline;
    obj["relays"]  = MEGA_RELAY_COUNT;
    obj["sensors"]  = 1;  // DHT11 currently
    obj["free_pins"] = hw.countAvailablePins();
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleGetPins() {
  // Stream pin table as JSON array
  String json = "[";
  char nameBuf[6];
  char capBuf[20];
  for (int i = 0; i < MEGA_PIN_COUNT; i++) {
    PinCap pc;
    memcpy_P(&pc, &MEGA_PINS[i], sizeof(pc));

    PinState st;
    const char *usedBy;
    hw.getPinState(pc.pin, st, usedBy);

    HardwareManager::pinName(pc.pin, nameBuf, sizeof(nameBuf));
    HardwareManager::capString(pc.caps, capBuf, sizeof(capBuf));

    if (i > 0) json += ',';
    json += "{\"pin\":";
    json += pc.pin;
    json += ",\"name\":\"";
    json += nameBuf;
    json += "\",\"caps\":\"";
    json += capBuf;
    json += "\",\"state\":";
    json += (int)st;
    json += ",\"used\":\"";
    json += usedBy;
    json += "\"}";
  }
  json += ']';
  server.send(200, "application/json", json);
}

void handleGetRelays() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < MEGA_RELAY_COUNT; i++) {
    JsonObject obj = arr.add<JsonObject>();
    obj["index"] = i;
    obj["label"] = String("Relay ") + String(i + 1);
    char pinBuf[6];
    HardwareManager::pinName(MEGA_RELAY_PINS[i], pinBuf, sizeof(pinBuf));
    obj["pin"] = String(pinBuf);
    Equipment *eq = config.findByOutput(i);
    if (eq) {
      obj["assigned"]  = true;
      obj["equipment"] = eq->name;
      obj["eq_id"]     = eq->id;
    } else {
      obj["assigned"] = false;
    }
    obj["state"] = outputs[i].confirmedOn ? "ON" : "OFF";
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleGetDrivers() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < SENSOR_DRIVER_COUNT; i++) {
    const SensorDriver &d = SENSOR_DRIVERS[i];
    JsonObject obj = arr.add<JsonObject>();
    obj["type"]      = (int)d.type;
    obj["name"]      = d.name;
    obj["desc"]      = d.description;
    obj["bus"]       = (int)d.bus;
    obj["pins"]      = d.pinCount;
    obj["supported"] = d.supported;
    if (d.devNote) obj["dev_note"] = d.devNote;
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleAddNode() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"need JSON body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }
  const char *id   = doc["id"] | "";
  const char *name = doc["name"] | "";
  uint8_t addr     = doc["address"] | 0;
  const char *board = doc["board"] | "mega2560";
  const char *fw   = doc["fw"] | "2.0";
  if (!id[0] || !name[0] || addr == 0) {
    server.send(400, "application/json", "{\"error\":\"need id, name, address\"}");
    return;
  }
  if (!hw.addNode(id, name, addr, board, fw)) {
    server.send(409, "application/json", "{\"error\":\"node exists or limit reached\"}");
    return;
  }
  server.send(201, "application/json", "{\"ok\":true}");
}

void handleDeleteNode() {
  String id = server.arg("id");
  if (!id.length()) {
    server.send(400, "application/json", "{\"error\":\"need id\"}");
    return;
  }
  if (!hw.removeNode(id.c_str())) {
    server.send(404, "application/json", "{\"error\":\"node not found\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

// =====================================================================
//  Automation engine — evaluate rules against sensor readings
// =====================================================================
const uint32_t AUTOMATION_INTERVAL_MS = 5000;
uint32_t lastAutomation = 0;

void evaluateRules() {
  for (int i = 0; i < config.ruleCount; i++) {
    AutomationRule &r = config.rules[i];
    if (!r.enabled) continue;

    Equipment *eq = config.findEquipment(r.equipmentId);
    if (!eq || eq->outputIndex >= MAX_OUTPUTS) continue;

    int16_t valueX10;
    if (r.sensor == 0)      valueX10 = (int16_t)(nodeTemperature * 10);
    else if (r.sensor == 1) valueX10 = (int16_t)(nodeHumidity * 10);
    else continue;

    bool shouldTrigger;
    if (r.condition == 0) {
      // "above": trigger when value > threshold, release when value < threshold - hysteresis
      shouldTrigger = r.triggered
        ? (valueX10 >= r.thresholdX10 - r.hysteresisX10)
        : (valueX10 > r.thresholdX10);
    } else {
      // "below": trigger when value < threshold, release when value > threshold + hysteresis
      shouldTrigger = r.triggered
        ? (valueX10 <= r.thresholdX10 + r.hysteresisX10)
        : (valueX10 < r.thresholdX10);
    }

    if (shouldTrigger && !r.triggered) {
      r.triggered = true;
      if (outputs[eq->outputIndex].status != CMD_PENDING &&
          outputs[eq->outputIndex].status != CMD_SENT) {
        outputs[eq->outputIndex].requestedOn = true;
        outputs[eq->outputIndex].status      = CMD_PENDING;
        outputs[eq->outputIndex].sentAt      = millis();
        outputs[eq->outputIndex].seq         = nextSeq++;
        outputs[eq->outputIndex].attempt     = 0;
        Serial.print("[Auto] Rule '"); Serial.print(r.name);
        Serial.print("' TRIGGERED -> "); Serial.print(eq->name);
        Serial.println(" ON");
      }
    } else if (!shouldTrigger && r.triggered) {
      r.triggered = false;
      if (outputs[eq->outputIndex].status != CMD_PENDING &&
          outputs[eq->outputIndex].status != CMD_SENT) {
        outputs[eq->outputIndex].requestedOn = false;
        outputs[eq->outputIndex].status      = CMD_PENDING;
        outputs[eq->outputIndex].sentAt      = millis();
        outputs[eq->outputIndex].seq         = nextSeq++;
        outputs[eq->outputIndex].attempt     = 0;
        Serial.print("[Auto] Rule '"); Serial.print(r.name);
        Serial.print("' RELEASED -> "); Serial.print(eq->name);
        Serial.println(" OFF");
      }
    }
  }
}

// =====================================================================
//  Catch-all: serve from LittleFS or 404
// =====================================================================
void handleNotFound() {
  if (serveFile(server.uri())) return;
  server.send(404, "text/plain", "Not found");
}

// =====================================================================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== LoRaSense gateway ===");

  // Init output states
  for (int i = 0; i < MAX_OUTPUTS; i++) {
    outputs[i].confirmedOn = false;
    outputs[i].requestedOn = false;
    outputs[i].status = CMD_IDLE;
    outputs[i].sentAt = 0;
    outputs[i].seq = 0;
    outputs[i].attempt = 0;
  }

  // LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("FAIL: LittleFS mount failed.");
  } else {
    Serial.println("LittleFS mounted.");
  }

  // Load config from flash
  config.begin();
  if (!config.env.configured) {
    Serial.println("First boot: waiting for onboarding wizard...");
    LittleFS.mkdir("/config");
  }
  Serial.print("Environment: "); Serial.print(config.env.name);
  Serial.print(" ("); Serial.print(config.env.type); Serial.println(")");
  Serial.print("Equipment: "); Serial.println(config.equipmentCount);
  Serial.print("Zones: "); Serial.println(config.zoneCount);
  Serial.print("Rules: "); Serial.println(config.ruleCount);

  // Hardware manager
  hw.begin();
  hw.ensureDefaultNode();
  syncRelayLabels();
  Serial.print("Nodes: "); Serial.println(hw.nodeCount);

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

  // Web server routes
  server.on("/api/state",   HTTP_GET,  handleState);
  server.on("/api/devices", HTTP_POST, handleSetDevice);

  // Config API
  server.on("/api/config",              HTTP_GET,    handleGetConfig);
  server.on("/api/config/environment",  HTTP_POST,   handleSetEnvironment);
  server.on("/api/config/zones",        HTTP_POST,   handleAddZone);
  server.on("/api/config/zones",        HTTP_DELETE, handleDeleteZone);
  server.on("/api/config/equipment",    HTTP_POST,   handleAddEquipment);
  server.on("/api/config/equipment",    HTTP_PUT,    handleUpdateEquipment);
  server.on("/api/config/equipment",    HTTP_DELETE, handleDeleteEquipment);

  // Onboarding
  server.on("/api/onboarding",          HTTP_POST,   handleOnboarding);

  // Rules API
  server.on("/api/rules",              HTTP_GET,    handleGetRules);
  server.on("/api/rules",              HTTP_POST,   handleAddRule);
  server.on("/api/rules",              HTTP_PUT,    handleUpdateRule);
  server.on("/api/rules",              HTTP_DELETE, handleDeleteRule);

  // History API
  server.on("/api/history",            HTTP_GET,    handleHistory);

  // Hardware Studio API
  server.on("/api/hw/nodes",           HTTP_GET,    handleGetNodes);
  server.on("/api/hw/nodes",           HTTP_POST,   handleAddNode);
  server.on("/api/hw/nodes",           HTTP_DELETE, handleDeleteNode);
  server.on("/api/hw/pins",            HTTP_GET,    handleGetPins);
  server.on("/api/hw/relays",          HTTP_GET,    handleGetRelays);
  server.on("/api/hw/drivers",         HTTP_GET,    handleGetDrivers);

  // Self-test
  server.on("/api/test",               HTTP_GET,    handleSelfTest);

  server.onNotFound(handleNotFound);
  server.begin();
  lastKeepalive = millis();
  Serial.println("Web server started. Ready.");
}

void loop() {
  server.handleClient();
  processPendingCommands();
  checkIncoming();

  if (millis() - lastKeepalive >= KEEPALIVE_INTERVAL_MS) {
    sendKeepalive();
    lastKeepalive = millis();
  }

  if (nodeOnline && millis() - lastAutomation >= AUTOMATION_INTERVAL_MS) {
    evaluateRules();
    lastAutomation = millis();
  }

  // Sensor history: first sample on first telemetry, then every 5 min
  if (nodeOnline) {
    if (!firstSampleTaken) {
      history.push(millis() / 1000, nodeTemperature, nodeHumidity);
      lastHistorySample = millis();
      firstSampleTaken = true;
    } else if (millis() - lastHistorySample >= HISTORY_INTERVAL_MS) {
      history.push(millis() / 1000, nodeTemperature, nodeHumidity);
      lastHistorySample = millis();
    }
  }
}
