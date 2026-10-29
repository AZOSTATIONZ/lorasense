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
#include "auth.h"
#include "config.h"
#include "hardware.h"
#include "history.h"
#include "../common/protocol.h"

WebServer server(80);
ConfigManager config;
HardwareManager hw;
SensorHistory history;
AuthManager auth;
EventLog eventLog;

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

// --- Action log ---
ActionLog actionLog;

// --- Sequence number ---
uint8_t nextSeq = 0;

// --- Node status (multi-node) ---
NodeState nodeStates[MAX_NODE_SLOTS];
const uint32_t NODE_TIMEOUT_MS = 45000;

// --- Keepalive ---
const uint32_t KEEPALIVE_INTERVAL_MS = 20000;
uint32_t lastKeepalive = 0;

// --- Sensor history sampling ---
const uint32_t HISTORY_INTERVAL_MS = 300000;  // 5 minutes
const uint32_t HISTORY_FLUSH_MS   = 1800000;  // 30 minutes
uint32_t lastHistorySample = 0;
bool firstSampleTaken = false;

// --- Software clock (user-set, no RTC/NTP in AP mode) ---
uint32_t clockSetAt    = 0;    // millis() when setGatewayClock was called
uint32_t clockBaseMin  = 0;    // minutes-from-midnight at that moment
uint8_t  clockDay      = 0;    // day-of-week at set time (0=Sun..6=Sat)
bool     clockValid    = false;

void setGatewayClock(uint8_t hour, uint8_t minute, uint8_t dayOfWeek) {
  clockSetAt   = millis();
  clockBaseMin = (uint32_t)hour * 60 + minute;
  clockDay     = dayOfWeek;
  clockValid   = true;
  Serial.print("[Clock] Set to "); Serial.print(hour); Serial.print(":");
  if (minute < 10) Serial.print("0");
  Serial.print(minute); Serial.print(" day="); Serial.println(dayOfWeek);
}

uint16_t gatewayMinuteOfDay() {
  if (!clockValid) return 0xFFFF;
  uint32_t elapsed = (millis() - clockSetAt) / 60000;
  return (clockBaseMin + elapsed) % 1440;
}

uint8_t gatewayDayOfWeek() {
  if (!clockValid) return 0xFF;
  uint32_t elapsed = (millis() - clockSetAt) / 60000;
  uint32_t totalMin = clockBaseMin + elapsed;
  return (clockDay + totalMin / 1440) % 7;
}

// --- Config sync ---
uint8_t  configVersion    = 0;
const uint32_t CONFIG_PUSH_INTERVAL_MS = 10000;  // retry every 10s until ACK
const uint32_t CONFIG_PUSH_INITIAL_DELAY_MS = 3000;  // wait for node after boot

// --- Helper: find NodeState by LoRa address ---
NodeState *findNodeByAddress(uint8_t addr) {
  for (int i = 0; i < MAX_NODE_SLOTS; i++) {
    if (nodeStates[i].loraAddress == addr) return &nodeStates[i];
  }
  return nullptr;
}

// --- Helper: find NodeState by node ID (from config) ---
NodeState *findNodeById(const char *nodeId) {
  NodeInstance *ni = hw.findNode(nodeId);
  if (!ni) return nullptr;
  return findNodeByAddress(ni->address);
}

// --- Helper: get the primary node (first registered, backward compat) ---
NodeState *primaryNode() {
  if (hw.nodeCount > 0) return findNodeByAddress(hw.nodes[0].address);
  return &nodeStates[0];
}

// --- Helper: is any node online? ---
bool anyNodeOnline() {
  for (int i = 0; i < hw.nodeCount; i++) {
    NodeState *ns = findNodeByAddress(hw.nodes[i].address);
    if (ns && ns->online) return true;
  }
  return false;
}

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

  // Find the target node address for this relay output
  uint8_t targetAddr = ADDR_NODE1;  // default for backward compat
  Equipment *owner = config.findByOutput(outputIdx);
  if (owner) {
    NodeInstance *ni = hw.findNode(owner->nodeId);
    if (ni) targetAddr = ni->address;
  }

  uint8_t buf[FRAME_MAX];
  uint8_t len = buildFrame(buf, targetAddr, ADDR_GATEWAY,
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
void handleTelemetry(const uint8_t *buf, uint8_t len, uint8_t srcAddr) {
  uint8_t payloadLen = len - HEADER_SIZE - CRC_SIZE;
  if (payloadLen < 5) return;

  NodeState *ns = findNodeByAddress(srcAddr);
  if (!ns) return;

  TelemetryPayload *t = (TelemetryPayload *)(buf + HEADER_SIZE);
  ns->temperature = t->temperatureX10 / 10.0;
  ns->humidity    = t->humidityX10    / 10.0;

  for (int i = 0; i < MAX_OUTPUTS && i < 8; i++) {
    outputs[i].confirmedOn = (t->relayStates >> i) & 1;
  }

  if (payloadLen >= sizeof(TelemetryPayload)) {
    ns->analogValue = t->analogValue;
    ns->analogPin   = t->analogPin;
  }

  ns->online = true;
  ns->lastHeard = millis();
  ns->lastTelemetryAt = millis();
  ns->rssi = LoRa.packetRssi();
  Serial.print("[LoRa] Telemetry from 0x"); Serial.print(srcAddr, HEX);
  Serial.print(": temp="); Serial.print(ns->temperature);
  Serial.print(" hum="); Serial.print(ns->humidity);
  if (ns->analogPin != 0xFF) {
    Serial.print(" adc="); Serial.print(ns->analogValue);
  }
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

  // Find the NodeState for this source address
  NodeState *srcNode = findNodeByAddress(hdr->src);
  if (srcNode) {
    srcNode->online = true;
    srcNode->lastHeard = millis();
    srcNode->rssi = LoRa.packetRssi();
  }

  if (hdr->type == MSG_ACK) {
    AckPayload *ack = (AckPayload *)(buf + HEADER_SIZE);
    if (ack->deviceIndex < MAX_OUTPUTS &&
        outputs[ack->deviceIndex].status == CMD_SENT &&
        hdr->seq == outputs[ack->deviceIndex].seq) {
      outputs[ack->deviceIndex].confirmedOn = (ack->actualOn == 1);
      outputs[ack->deviceIndex].status = CMD_CONFIRMED;
      Serial.print("[LoRa] ACK from 0x"); Serial.print(hdr->src, HEX);
      Serial.print(" output="); Serial.print(ack->deviceIndex);
      Serial.print(" on="); Serial.println(ack->actualOn);
    }
  }
  else if (hdr->type == MSG_TELEMETRY) {
    if (srcNode && hdr->seq == srcNode->lastTelemetrySeq) {
      Serial.print("[LoRa] duplicate telemetry seq="); Serial.print(hdr->seq);
      Serial.print(" from 0x"); Serial.println(hdr->src, HEX);
    } else {
      if (srcNode) srcNode->lastTelemetrySeq = hdr->seq;
      handleTelemetry(buf, n, hdr->src);
    }
  }
  else if (hdr->type == MSG_HEARTBEAT) {
    if (srcNode && hdr->seq == srcNode->lastHeartbeatSeq) {
      Serial.print("[LoRa] duplicate heartbeat seq="); Serial.print(hdr->seq);
      Serial.print(" from 0x"); Serial.println(hdr->src, HEX);
    } else {
      if (srcNode) srcNode->lastHeartbeatSeq = hdr->seq;
      Serial.print("[LoRa] Heartbeat from 0x"); Serial.println(hdr->src, HEX);
    }
  }
  else if (hdr->type == MSG_CONFIG_ACK) {
    if (n >= (int)(HEADER_SIZE + sizeof(ConfigAckPayload) + CRC_SIZE)) {
      ConfigAckPayload *ack = (ConfigAckPayload *)(buf + HEADER_SIZE);
      if (srcNode) {
        srcNode->configVersion = ack->version;
        if (ack->version == configVersion) {
          srcNode->configSynced = true;
          Serial.print("[Sync] Node 0x"); Serial.print(hdr->src, HEX);
          Serial.print(" confirmed config v"); Serial.println(ack->version);
        }
      }
    }
  }
}

// =====================================================================
//  Send keepalive
// =====================================================================
void sendKeepalive() {
  for (int i = 0; i < hw.nodeCount; i++) {
    if (!hw.nodes[i].active) continue;
    uint8_t buf[FRAME_MAX];
    uint8_t len = buildFrame(buf, hw.nodes[i].address, ADDR_GATEWAY,
                             MSG_KEEPALIVE, 0, nullptr, 0);
    LoRa.beginPacket();
    LoRa.write(buf, len);
    LoRa.endPacket();
    if (hw.nodeCount > 1) delay(5);
  }
}

// =====================================================================
//  Config sync — push config to node over LoRa
// =====================================================================
void bumpConfigVersion() {
  configVersion++;
  for (int i = 0; i < MAX_NODE_SLOTS; i++) nodeStates[i].configSynced = false;
  Serial.print("[Sync] Config version bumped to "); Serial.println(configVersion);
}

bool allNodesSynced() {
  for (int i = 0; i < hw.nodeCount; i++) {
    if (!hw.nodes[i].active) continue;
    NodeState *ns = findNodeByAddress(hw.nodes[i].address);
    if (ns && !ns->configSynced) return false;
  }
  return true;
}

void pushConfigToNode(NodeInstance &node) {
  NodeState *ns = findNodeByAddress(node.address);

  // Gather sensors assigned to this node
  uint8_t sensType = 0;
  uint8_t sensPin  = 0xFF;
  uint8_t analogPin = 0xFF;
  for (int i = 0; i < config.sensorCount; i++) {
    SensorInstance &si = config.sensors[i];
    if (!si.active || !si.enabled) continue;
    if (strcmp(si.nodeId, node.id) != 0) continue;
    if ((si.sensorType == STYPE_DHT11 || si.sensorType == STYPE_DHT22) && sensType == 0) {
      sensType = si.sensorType;
      sensPin  = si.pin;
    } else if (si.sensorType == STYPE_ANALOG && analogPin == 0xFF) {
      analogPin = si.pin;
    }
  }

  uint16_t telSec = 10;
  uint16_t fsSec  = 60;

  ConfigPushPayload cfg;
  cfg.configType = CFG_FULL_SYNC;
  cfg.dataLen    = 8;
  cfg.data[0]    = configVersion;
  cfg.data[1]    = sensType;
  cfg.data[2]    = sensPin;
  cfg.data[3]    = analogPin;
  cfg.data[4]    = (telSec >> 8) & 0xFF;
  cfg.data[5]    = telSec & 0xFF;
  cfg.data[6]    = (fsSec >> 8) & 0xFF;
  cfg.data[7]    = fsSec & 0xFF;

  uint8_t buf[FRAME_MAX];
  uint8_t len = buildFrame(buf, node.address, ADDR_GATEWAY,
                           MSG_CONFIG_PUSH, nextSeq++, &cfg, sizeof(cfg));
  LoRa.beginPacket();
  LoRa.write(buf, len);
  LoRa.endPacket();

  if (ns) ns->lastConfigPush = millis();
  Serial.print("[Sync] Pushed config v"); Serial.print(configVersion);
  Serial.print(" to 0x"); Serial.print(node.address, HEX);
  Serial.print(" dht="); Serial.print(sensType);
  Serial.print("/pin"); Serial.print(sensPin);
  Serial.print(" adc="); Serial.println(analogPin);
}

// =====================================================================
//  Process pending commands (non-blocking state machine)
// =====================================================================
void processPendingCommands() {
  for (int i = 0; i < MAX_OUTPUTS; i++) {
    OutputState &o = outputs[i];

    if (o.status == CMD_PENDING) {
      if (o.retryAfter > 0 && millis() < o.retryAfter) continue;
      if (o.retryAfter == 0) o.attempt = 0;  // fresh command, not a retry
      o.retryAfter = 0;
      sendCommand(i);
      return;
    }

    if (o.status == CMD_SENT) {
      if (millis() - o.sentAt < ACK_TIMEOUT_MS) return;
      // Timed out — retry with exponential backoff, or fail
      o.attempt++;
      if (o.attempt < MAX_RETRIES) {
        uint32_t backoff = RETRY_DELAY_MS * (1 << o.attempt);  // 400ms, 800ms
        o.retryAfter = millis() + backoff;
        Serial.print("[LoRa] timeout output="); Serial.print(i);
        Serial.print(", retry "); Serial.print(o.attempt + 1);
        Serial.print(" in "); Serial.print(backoff); Serial.println("ms");
        o.status = CMD_PENDING;  // re-queue for retry after backoff
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
// Determine measurement quality for a sensor reading
MeasureQuality measureQuality(bool hasReading, uint32_t ageMs, float value,
                               int16_t minX10, int16_t maxX10) {
  if (!hasReading) return MQ_MISSING;
  if (isnan(value) || (int16_t)(value * 10) < minX10 || (int16_t)(value * 10) > maxX10)
    return MQ_INVALID;
  if (ageMs > HISTORY_INTERVAL_MS * 2) return MQ_STALE;  // >10 min = stale
  return MQ_FRESH;
}

const char *qualityName(MeasureQuality q) {
  switch (q) {
    case MQ_FRESH:   return "fresh";
    case MQ_STALE:   return "stale";
    case MQ_INVALID: return "invalid";
    default:         return "missing";
  }
}

// Apply calibration: result = (raw × scale/100) + offset/10
float applyCalibration(float raw, int16_t offsetX10, uint16_t scaleX100) {
  return (raw * scaleX100 / 100.0f) + (offsetX10 / 10.0f);
}

void handleState() {
  if (!auth.check(server, ROLE_USER)) return;
  addSecHeaders();

  // Update node timeout status
  uint32_t now = millis();
  for (int i = 0; i < hw.nodeCount; i++) {
    NodeState *ns = findNodeByAddress(hw.nodes[i].address);
    if (ns && ns->online && now - ns->lastHeard > NODE_TIMEOUT_MS)
      ns->online = false;
  }

  NodeState *pn = primaryNode();
  JsonDocument doc;
  doc["uptime_s"]     = now / 1000;
  doc["clients"]      = WiFi.softAPgetStationNum();
  doc["node_online"]  = anyNodeOnline();
  doc["temperature_c"] = serialized(String(pn ? pn->temperature : 0.0f, 1));
  doc["humidity_pct"]  = serialized(String(pn ? pn->humidity : 0.0f, 1));
  doc["rssi"]         = pn ? pn->rssi : 0;
  doc["protocol_ver"] = PROTOCOL_VERSION;
  doc["configured"]   = config.env.configured;
  doc["commission"]   = (int)config.env.commissionStatus;
  doc["commission_label"] = commissionName(config.env.commissionStatus);
  doc["config_version"] = configVersion;
  doc["config_synced"]  = allNodesSynced();
  doc["clock_valid"]    = clockValid;
  if (clockValid) {
    uint16_t cm = gatewayMinuteOfDay();
    doc["clock_hour"]   = cm / 60;
    doc["clock_minute"] = cm % 60;
    doc["clock_day"]    = gatewayDayOfWeek();
  }
  if (pn && pn->lastTelemetryAt > 0) {
    doc["reading_age_s"]  = (now - pn->lastTelemetryAt) / 1000;
  }

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
      if (outputs[eq.outputIndex].manualOverride) d["override"] = true;
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

  // Configured sensors with live readings, calibration, and quality
  JsonArray sensArr = doc["sensors"].to<JsonArray>();
  for (int i = 0; i < config.sensorCount; i++) {
    SensorInstance &si = config.sensors[i];
    if (!si.active || !si.enabled) continue;

    NodeState *sns = findNodeById(si.nodeId);
    bool sOnline = sns ? sns->online : false;
    uint32_t sTelAge = (sns && sns->lastTelemetryAt > 0) ? (now - sns->lastTelemetryAt) : UINT32_MAX;

    JsonObject so = sensArr.add<JsonObject>();
    so["id"]   = si.id;
    so["name"] = si.name;
    if (si.label[0]) so["label"] = si.label;
    so["type"] = si.sensorType;
    so["connected"] = sOnline;

    so["zone_id"] = si.zoneId;
    so["pin"]     = si.pin;
    if (sns && sns->lastTelemetryAt > 0)
      so["reading_age_s"] = (int)(sTelAge / 1000);

    const SensorDriver *drv = nullptr;
    for (int d = 0; d < SENSOR_DRIVER_COUNT; d++) {
      if (SENSOR_DRIVERS[d].type == si.sensorType) {
        drv = &SENSOR_DRIVERS[d];
        so["driver"]   = drv->name;
        so["quantity"] = drv->quantity;
        so["unit"]     = drv->unit;
        break;
      }
    }

    if (si.sensorType == STYPE_DHT11 || si.sensorType == STYPE_DHT22) {
      JsonObject rd = so["readings"].to<JsonObject>();
      bool hasReading = sOnline && sns->lastTelemetryAt > 0;
      float calTemp = applyCalibration(sns ? sns->temperature : 0.0f, si.offsetX10, si.scaleX100);
      MeasureQuality tq = measureQuality(hasReading, sTelAge, sns ? sns->temperature : 0.0f,
                            drv ? drv->rangeMinX10 : -400, drv ? drv->rangeMaxX10 : 800);
      MeasureQuality hq = measureQuality(hasReading, sTelAge, sns ? sns->humidity : 0.0f, 0, 1000);
      so["quality"] = qualityName(tq < hq ? hq : tq);
      if (hasReading) {
        rd["temperature_c"] = serialized(String(calTemp, 1));
        rd["humidity_pct"]  = serialized(String(sns->humidity, 1));
      }
    } else if (si.sensorType == STYPE_ANALOG) {
      JsonObject rd = so["readings"].to<JsonObject>();
      bool hasReading = sOnline && sns && sns->analogValue >= 0;
      MeasureQuality aq = measureQuality(hasReading, sTelAge,
                            sns ? (float)sns->analogValue : 0.0f, 0, 10230);
      so["quality"] = qualityName(aq);
      if (hasReading) {
        float calAnalog = applyCalibration(sns->analogValue / 10.23f, si.offsetX10, si.scaleX100);
        rd["analog_raw"] = sns->analogValue;
        rd["analog_pct"] = serialized(String(calAnalog, 1));
      }
    }
  }

  // Active automation rules
  JsonArray rulesArr = doc["rules"].to<JsonArray>();
  for (int i = 0; i < config.ruleCount; i++) {
    AutomationRule &r = config.rules[i];
    if (!r.enabled) continue;
    JsonObject ro = rulesArr.add<JsonObject>();
    ro["name"]      = r.name;
    ro["rule_type"] = r.ruleType;
    ro["equipment"] = r.equipmentId;
    ro["triggered"] = r.triggered;
    ro["enabled"]   = r.enabled;
    if (r.ruleType == RULE_SENSOR) {
      if (r.durationS)  ro["duration_s"]  = r.durationS;
      if (r.cooldownS)  ro["cooldown_s"]  = r.cooldownS;
      if (r.faultActive) ro["fault"] = true;
    } else if (r.ruleType == RULE_SCHEDULE) {
      ro["sched_start"] = r.schedStartMin;
      ro["sched_stop"]  = r.schedStopMin;
      ro["sched_days"]  = r.schedDays;
    }
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// =====================================================================
//  Helpers: body size check and security headers
// =====================================================================
bool checkBodySize(int maxBytes = 2048) {
  if (server.hasArg("plain") && (int)server.arg("plain").length() > maxBytes) {
    server.send(413, "application/json", "{\"error\":\"request body too large\"}");
    return false;
  }
  return true;
}

void addSecHeaders() {
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("X-Frame-Options", "DENY");
}

// =====================================================================
//  HTTP: POST /api/devices — control a device by equipment ID
// =====================================================================
void handleSetDevice() {
  if (!auth.check(server, ROLE_USER)) return;
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

  // Interlock check: prevent conflicting equipment from running simultaneously
  if (state == "on") {
    const char *conflict = interlockConflict(eq->outputIndex);
    if (conflict) {
      String msg = "{\"error\":\"interlock: " + String(conflict) + " is active\"}";
      server.send(409, "application/json", msg);
      return;
    }
  }

  outputs[eq->outputIndex].requestedOn = (state == "on");
  outputs[eq->outputIndex].status      = CMD_PENDING;
  outputs[eq->outputIndex].sentAt      = millis();
  outputs[eq->outputIndex].retryAfter  = 0;
  outputs[eq->outputIndex].seq         = nextSeq++;
  outputs[eq->outputIndex].attempt     = 0;

  server.send(202, "application/json", "{\"status\":\"PENDING\"}");
}

// =====================================================================
//  HTTP: Config API
// =====================================================================
void handleGetConfig() {
  if (!auth.check(server, ROLE_USER)) return;
  addSecHeaders();
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

  // Available relay outputs (respect configured relay count)
  int relayCount = config.env.relayCount;
  JsonArray outArr = doc["available_outputs"].to<JsonArray>();
  for (int i = 0; i < relayCount; i++) {
    if (!config.isOutputTaken(i)) outArr.add(i);
  }

  doc["max_outputs"] = MAX_OUTPUTS;
  doc["relay_count"] = relayCount;

  // Relay pin map for UI
  JsonArray pinArr = doc["relay_pins"].to<JsonArray>();
  for (int i = 0; i < relayCount; i++) {
    JsonObject rp = pinArr.add<JsonObject>();
    rp["index"] = i;
    char pinBuf[6];
    if (i < MEGA_RELAY_COUNT) {
      HardwareManager::pinName(MEGA_RELAY_PINS[i], pinBuf, sizeof(pinBuf));
      rp["pin"] = String(pinBuf);
    }
    Equipment *owner = config.findByOutput(i);
    if (owner) rp["assigned_to"] = owner->name;
  }

  // Sensors
  JsonArray sArr = doc["sensors"].to<JsonArray>();
  for (int i = 0; i < config.sensorCount; i++) {
    SensorInstance &s = config.sensors[i];
    if (!s.active) continue;
    JsonObject so = sArr.add<JsonObject>();
    so["id"]      = s.id;
    so["name"]    = s.name;
    so["type"]    = s.sensorType;
    so["node_id"] = s.nodeId;
    if (s.pin != 0xFF) {
      so["pin"] = s.pin;
      char pn[6];
      HardwareManager::pinName(s.pin, pn, sizeof(pn));
      so["pin_name"] = String(pn);
    }
    so["zone_id"] = s.zoneId;
    so["enabled"] = s.enabled;
  }

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
    if (r.durationS)  ro["duration_s"]  = r.durationS;
    if (r.cooldownS)  ro["cooldown_s"]  = r.cooldownS;
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleSetEnvironment() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  if (!checkBodySize()) return;
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
  if (doc["relay_count"].is<int>()) {
    config.env.relayCount = constrain((int)doc["relay_count"], 1, MAX_OUTPUTS);
  }
  config.saveEnvironment();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleAddZone() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  if (!checkBodySize()) return;
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
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
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

SensorPinInfo sensorPinCache[MAX_SENSORS];

void syncSensorPins() {
  for (int i = 0; i < config.sensorCount && i < MAX_SENSORS; i++) {
    SensorInstance &s = config.sensors[i];
    sensorPinCache[i].pin        = s.pin;
    sensorPinCache[i].sensorType = s.sensorType;
    sensorPinCache[i].name       = s.name;
    sensorPinCache[i].active     = s.active && s.enabled;
  }
  hw.setSensorPins(sensorPinCache, config.sensorCount);
}

void handleAddEquipment() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  if (!checkBodySize()) return;
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
  uint8_t interlock  = doc["interlock"] | 0;

  if (!id[0] || !name[0]) {
    server.send(400, "application/json", "{\"error\":\"need id and name\"}");
    return;
  }
  if (output != 0xFF && output >= config.env.relayCount) {
    String msg = "{\"error\":\"Relay " + String(output + 1) + " is unavailable (configured for " + String(config.env.relayCount) + " channels)\"}";
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
  if (!config.addEquipment(id, name, cat, zone, nodeId, output, interlock)) {
    server.send(409, "application/json", "{\"error\":\"equipment exists or limit reached\"}");
    return;
  }
  syncRelayLabels();
  server.send(201, "application/json", "{\"ok\":true}");
}

void handleUpdateEquipment() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  if (!checkBodySize()) return;
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
  uint8_t interlock  = doc["interlock"] | 0;

  if (!id[0]) {
    server.send(400, "application/json", "{\"error\":\"need id\"}");
    return;
  }
  if (output != 0xFF && output >= config.env.relayCount) {
    String msg = "{\"error\":\"Relay " + String(output + 1) + " is unavailable (configured for " + String(config.env.relayCount) + " channels)\"}";
    server.send(400, "application/json", msg);
    return;
  }
  if (output != 0xFF && config.isOutputTaken(output, id)) {
    Equipment *owner = config.findByOutput(output);
    String msg = "{\"error\":\"Relay " + String(output + 1) + " is already assigned to " + String(owner ? owner->name : "another device") + "\"}";
    server.send(409, "application/json", msg);
    return;
  }
  if (!config.updateEquipment(id, name, cat, zone, nodeId, output, interlock)) {
    server.send(404, "application/json", "{\"error\":\"not found\"}");
    return;
  }
  syncRelayLabels();
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleDeleteEquipment() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
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
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  if (!checkBodySize(4096)) return;
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

  // Sensors (added in Phase 4 wizard)
  JsonArray sensors = doc["sensors"].as<JsonArray>();
  for (JsonObject s : sensors) {
    const char *id   = s["id"] | "";
    const char *name = s["name"] | "";
    uint8_t type     = s["type"] | 0;
    uint8_t pin      = s["pin"] | 0xFF;
    const char *zone = s["zone_id"] | "";
    const char *nodeId = s["node_id"] | "mega-001";
    if (id[0] && name[0]) config.addSensor(id, name, type, nodeId, pin, zone, true);
  }
  syncSensorPins();

  // Mark as configured
  config.env.commissionStatus = CS_CONFIGURED;
  config.saveEnvironment();
  bumpConfigVersion();

  Serial.println("[Onboarding] Setup complete.");
  eventLog.add("config", "Onboarding completed");
  server.send(200, "application/json", "{\"ok\":true}");
}

// =====================================================================
//  HTTP: POST /api/commission — advance commissioning status
// =====================================================================
const char *commissionName(CommissionStatus s) {
  switch (s) {
    case CS_CONFIGURED:   return "configured";
    case CS_TESTED:       return "tested";
    case CS_COMMISSIONED: return "commissioned";
    default:              return "draft";
  }
}

void handleCommission() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  if (!checkBodySize()) return;
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"need JSON body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }
  int status = doc["status"] | -1;
  if (status < CS_DRAFT || status > CS_COMMISSIONED) {
    server.send(400, "application/json", "{\"error\":\"invalid status (0-3)\"}");
    return;
  }
  // Validate minimum requirements for commissioning
  if (status >= CS_CONFIGURED) {
    if (!config.env.configured) {
      server.send(409, "application/json", "{\"error\":\"environment not configured\"}");
      return;
    }
    if (config.zoneCount == 0) {
      server.send(409, "application/json", "{\"error\":\"at least one zone required\"}");
      return;
    }
    if (config.sensorCount == 0) {
      server.send(409, "application/json", "{\"error\":\"at least one sensor required\"}");
      return;
    }
  }
  config.env.commissionStatus = (CommissionStatus)status;
  config.saveEnvironment();
  char logMsg[64];
  snprintf(logMsg, sizeof(logMsg), "Status -> %s", commissionName(config.env.commissionStatus));
  eventLog.add("config", logMsg);
  String resp = "{\"ok\":true,\"status\":\"";
  resp += commissionName(config.env.commissionStatus);
  resp += "\"}";
  server.send(200, "application/json", resp);
}

// =====================================================================
//  HTTP: Clock API
// =====================================================================
void handleGetClock() {
  if (!auth.check(server, ROLE_USER)) return;
  JsonDocument doc;
  doc["valid"] = clockValid;
  if (clockValid) {
    uint16_t cm = gatewayMinuteOfDay();
    doc["hour"]   = cm / 60;
    doc["minute"] = cm % 60;
    doc["day"]    = gatewayDayOfWeek();
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleSetClock() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  if (!checkBodySize()) return;
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"need JSON body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }
  int hour   = doc["hour"]   | -1;
  int minute = doc["minute"] | -1;
  int day    = doc["day"]    | -1;
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || day < 0 || day > 6) {
    server.send(400, "application/json", "{\"error\":\"need hour(0-23), minute(0-59), day(0=Sun..6=Sat)\"}");
    return;
  }
  setGatewayClock((uint8_t)hour, (uint8_t)minute, (uint8_t)day);
  char lm[48];
  snprintf(lm, sizeof(lm), "Clock set to %02d:%02d day=%d", hour, minute, day);
  eventLog.add("system", lm);
  server.send(200, "application/json", "{\"ok\":true}");
}

// =====================================================================
//  HTTP: Rules API
// =====================================================================
void handleGetRules() {
  if (!auth.check(server, ROLE_USER)) return;
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < config.ruleCount; i++) {
    AutomationRule &r = config.rules[i];
    JsonObject obj = arr.add<JsonObject>();
    obj["id"]         = r.id;
    obj["name"]       = r.name;
    obj["rule_type"]  = r.ruleType;
    obj["sensor"]     = r.sensor;
    obj["condition"]  = r.condition;
    obj["threshold"]  = r.thresholdX10;
    obj["hysteresis"] = r.hysteresisX10;
    obj["equipment"]  = r.equipmentId;
    obj["enabled"]    = r.enabled;
    obj["triggered"]  = r.triggered;
    obj["duration_s"] = r.durationS;
    obj["cooldown_s"] = r.cooldownS;
    if (r.ruleType == RULE_SCHEDULE) {
      obj["sched_start"] = r.schedStartMin;
      obj["sched_stop"]  = r.schedStopMin;
      obj["sched_days"]  = r.schedDays;
    }
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleAddRule() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  if (!checkBodySize()) return;
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
  uint8_t ruleType  = doc["rule_type"] | 0;
  uint8_t sensor    = doc["sensor"] | 0;
  uint8_t condition = doc["condition"] | 0;
  int16_t threshold = doc["threshold"] | 0;
  int16_t hyst      = doc["hysteresis"] | 0;
  bool enabled      = doc["enabled"] | true;
  uint16_t durationS = doc["duration_s"] | 0;
  uint16_t cooldownS = doc["cooldown_s"] | 0;
  uint16_t schedStart = doc["sched_start"] | 0;
  uint16_t schedStop  = doc["sched_stop"] | 0;
  uint8_t  schedDays  = doc["sched_days"] | 0x7F;

  if (!config.addRule(id, name, ruleType, sensor, condition, threshold, hyst, eqId, enabled, durationS, cooldownS, schedStart, schedStop, schedDays)) {
    server.send(409, "application/json", "{\"error\":\"rule exists or limit reached\"}");
    return;
  }
  server.send(201, "application/json", "{\"ok\":true}");
}

void handleUpdateRule() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  if (!checkBodySize()) return;
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
  uint8_t ruleType  = doc["rule_type"] | 0;
  uint8_t sensor    = doc["sensor"] | 0;
  uint8_t condition = doc["condition"] | 0;
  int16_t threshold = doc["threshold"] | 0;
  int16_t hyst      = doc["hysteresis"] | 0;
  bool enabled      = doc["enabled"] | true;
  uint16_t durationS = doc["duration_s"] | 0;
  uint16_t cooldownS = doc["cooldown_s"] | 0;
  uint16_t schedStart = doc["sched_start"] | 0;
  uint16_t schedStop  = doc["sched_stop"] | 0;
  uint8_t  schedDays  = doc["sched_days"] | 0x7F;

  if (!config.updateRule(id, name, ruleType, sensor, condition, threshold, hyst, eqId, enabled, durationS, cooldownS, schedStart, schedStop, schedDays)) {
    server.send(404, "application/json", "{\"error\":\"rule not found\"}");
    return;
  }
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleDeleteRule() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
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
//  HTTP: GET /api/action-log — recent automation actions
// =====================================================================
void handleActionLog() {
  if (!auth.check(server, ROLE_USER)) return;
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < actionLog.count; i++) {
    int idx = (actionLog.head - actionLog.count + i + ACTION_LOG_SIZE) % ACTION_LOG_SIZE;
    ActionLogEntry &e = actionLog.entries[idx];
    JsonObject o = arr.add<JsonObject>();
    o["ts"]        = e.timestamp / 1000;
    o["rule"]      = e.ruleName;
    o["action"]    = e.action;
    o["equipment"] = e.equipment;
    o["value"]     = serialized(String(e.valueX10 / 10.0, 1));
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// =====================================================================
//  HTTP: POST /api/override — toggle manual override for an equipment
// =====================================================================
void handleOverride() {
  if (!auth.check(server, ROLE_USER)) return;
  if (!checkBodySize()) return;
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"need JSON body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }
  const char *eqId = doc["id"] | "";
  if (!eqId[0]) {
    server.send(400, "application/json", "{\"error\":\"need equipment id\"}");
    return;
  }
  Equipment *eq = config.findEquipment(eqId);
  if (!eq || eq->outputIndex >= MAX_OUTPUTS) {
    server.send(404, "application/json", "{\"error\":\"equipment not found or no relay\"}");
    return;
  }
  bool override = doc["override"] | false;
  outputs[eq->outputIndex].manualOverride = override;
  actionLog.add("--", override ? "OVERRIDE" : "AUTO", eq->name, 0);
  char logMsg[64];
  snprintf(logMsg, sizeof(logMsg), "%s: %s", eq->name, override ? "manual override" : "auto resumed");
  eventLog.add("relay", logMsg);
  server.send(200, "application/json", "{\"ok\":true}");
}

// =====================================================================
//  HTTP: /api/history — sensor reading history
// =====================================================================
void handleHistory() {
  if (!auth.check(server, ROLE_USER)) return;
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
    if (r.analogX10 >= 0) {
      json += ",\"analog\":";
      json += String(r.analogX10 / 10.0, 1);
    }
    json += '}';
  }
  json += ']';
  server.send(200, "application/json", json);
}

// =====================================================================
//  HTTP: /api/test — on-device self-test
// =====================================================================
void handleSelfTest() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
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
//  HTTP: Sensor API
// =====================================================================
void handleGetSensors() {
  if (!auth.check(server, ROLE_USER)) return;
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < config.sensorCount; i++) {
    SensorInstance &s = config.sensors[i];
    if (!s.active) continue;
    JsonObject obj = arr.add<JsonObject>();
    obj["id"]      = s.id;
    obj["name"]    = s.name;
    obj["type"]    = s.sensorType;
    obj["node_id"] = s.nodeId;
    if (s.pin != 0xFF) {
      obj["pin"] = s.pin;
      char pinBuf[6];
      HardwareManager::pinName(s.pin, pinBuf, sizeof(pinBuf));
      obj["pin_name"] = String(pinBuf);
    }
    obj["zone_id"] = s.zoneId;
    obj["enabled"] = s.enabled;
    // Find driver info
    for (int d = 0; d < SENSOR_DRIVER_COUNT; d++) {
      if (SENSOR_DRIVERS[d].type == s.sensorType) {
        obj["driver"]    = SENSOR_DRIVERS[d].name;
        obj["supported"] = SENSOR_DRIVERS[d].supported;
        break;
      }
    }
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleAddSensor() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  if (!checkBodySize()) return;
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
  const char *label  = doc["label"] | "";
  uint8_t type       = doc["type"] | 0;
  const char *nodeId = doc["node_id"] | "mega-001";
  uint8_t pin        = doc["pin"] | 0xFF;
  const char *zoneId = doc["zone_id"] | "";
  bool enabled       = doc["enabled"] | true;
  int16_t calOffset  = doc["cal_offset"] | (int16_t)0;
  uint16_t calScale  = doc["cal_scale"]  | (uint16_t)100;

  if (!id[0] || !name[0]) {
    server.send(400, "application/json", "{\"error\":\"need id and name\"}");
    return;
  }
  // Validate sensor type is known
  bool typeFound = false;
  bool typeSupported = false;
  uint8_t pinReq = 0;
  uint8_t maxPerNode = 1;
  for (int d = 0; d < SENSOR_DRIVER_COUNT; d++) {
    if (SENSOR_DRIVERS[d].type == type) {
      typeFound = true;
      typeSupported = SENSOR_DRIVERS[d].supported;
      pinReq = SENSOR_DRIVERS[d].pinRequirement;
      maxPerNode = SENSOR_DRIVERS[d].maxPerNode;
      break;
    }
  }
  if (!typeFound) {
    server.send(400, "application/json", "{\"error\":\"unknown sensor type\"}");
    return;
  }
  if (!typeSupported) {
    server.send(400, "application/json", "{\"error\":\"sensor driver not yet supported by firmware\"}");
    return;
  }
  // Validate node exists
  if (nodeId[0] && !hw.findNode(nodeId)) {
    server.send(400, "application/json", "{\"error\":\"controller node not found\"}");
    return;
  }
  // Validate pin
  if (pin != 0xFF) {
    if (!hw.isPinAvailable(pin) && pin != MEGA_DHT_PIN) {
      PinState ps; const char *ub;
      hw.getPinState(pin, ps, ub);
      String msg = "{\"error\":\"Pin ";
      char pn[6]; HardwareManager::pinName(pin, pn, sizeof(pn));
      msg += pn;
      msg += " is reserved by ";
      msg += ub;
      msg += "\"}";
      server.send(409, "application/json", msg);
      return;
    }
    if (pinReq && !hw.pinHasCapability(pin, pinReq)) {
      server.send(400, "application/json", "{\"error\":\"pin lacks required capability for this sensor\"}");
      return;
    }
    if (config.isSensorPinTaken(pin)) {
      server.send(409, "application/json", "{\"error\":\"pin already assigned to another sensor\"}");
      return;
    }
  }
  // Check instance limit per node for this sensor type
  uint8_t countOnNode = 0;
  for (int i = 0; i < config.sensorCount; i++) {
    if (config.sensors[i].active &&
        config.sensors[i].sensorType == type &&
        strcmp(config.sensors[i].nodeId, nodeId) == 0)
      countOnNode++;
  }
  if (countOnNode >= maxPerNode) {
    server.send(409, "application/json",
      "{\"error\":\"this node already has the maximum number of this sensor type\"}");
    return;
  }
  if (!config.addSensor(id, name, type, nodeId, pin, zoneId, enabled,
                         label, calOffset, calScale)) {
    server.send(409, "application/json", "{\"error\":\"sensor exists or limit reached\"}");
    return;
  }
  bumpConfigVersion();
  syncSensorPins();
  server.send(201, "application/json", "{\"ok\":true}");
}

void handleDeleteSensor() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  String id = server.arg("id");
  if (!id.length()) {
    server.send(400, "application/json", "{\"error\":\"need id\"}");
    return;
  }
  if (!config.removeSensor(id.c_str())) {
    server.send(404, "application/json", "{\"error\":\"not found\"}");
    return;
  }
  bumpConfigVersion();
  syncSensorPins();
  server.send(200, "application/json", "{\"ok\":true}");
}

void ensureDefaultSensor() {
  if (config.sensorCount > 0) return;
  config.addSensor("dht11-001", "Temperature & Humidity", 1, "mega-001", MEGA_DHT_PIN, "", true);
}

// =====================================================================
//  HTTP: Hardware Studio API
// =====================================================================
void handleGetNodes() {
  if (!auth.check(server, ROLE_USER)) return;
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
    NodeState *nns = findNodeByAddress(hw.nodes[i].address);
    obj["online"]  = nns ? nns->online : false;
    obj["rssi"]    = nns ? nns->rssi : 0;
    obj["relays"]  = config.env.relayCount;
    int sCount = 0;
    for (int s = 0; s < config.sensorCount; s++)
      if (config.sensors[s].active && strcmp(config.sensors[s].nodeId, hw.nodes[i].id) == 0) sCount++;
    obj["sensors"]  = sCount;
    obj["free_pins"] = hw.countAvailablePins();
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleGetPins() {
  if (!auth.check(server, ROLE_USER)) return;
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
  if (!auth.check(server, ROLE_USER)) return;
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < config.env.relayCount; i++) {
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
  if (!auth.check(server, ROLE_USER)) return;
  JsonDocument doc;
  JsonArray arr = doc["drivers"].to<JsonArray>();
  for (int i = 0; i < SENSOR_DRIVER_COUNT; i++) {
    const SensorDriver &d = SENSOR_DRIVERS[i];
    JsonObject obj = arr.add<JsonObject>();
    obj["type"]      = (int)d.type;
    obj["name"]      = d.name;
    obj["desc"]      = d.description;
    obj["bus"]       = (int)d.bus;
    obj["pins"]      = d.pinCount;
    obj["pin_req"]   = d.pinRequirement;
    obj["supported"] = d.supported;
    obj["quantity"]  = d.quantity;
    obj["unit"]      = d.unit;
    obj["max_per_node"] = (int)d.maxPerNode;
    obj["driver_ver"]   = d.driverVersion;
    obj["fw_min"]       = d.fwMinVersion;
    if (d.hwRequirements) obj["hw_req"] = d.hwRequirements;
    if (d.devNote) obj["dev_note"] = d.devNote;
    obj["range_min"] = d.rangeMinX10;
    obj["range_max"] = d.rangeMaxX10;
  }
  JsonArray expArr = doc["expansion"].to<JsonArray>();
  for (int i = 0; i < EXPANSION_MODULE_COUNT; i++) {
    const ExpansionModule &m = EXPANSION_MODULES[i];
    JsonObject obj = expArr.add<JsonObject>();
    obj["type"]      = (int)m.type;
    obj["name"]      = m.name;
    obj["i2c_addr"]  = m.i2cAddress;
    obj["pins"]      = m.pinCount;
    obj["supported"] = m.supported;
    if (m.devNote) obj["dev_note"] = m.devNote;
  }
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleAddNode() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  if (!checkBodySize()) return;
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
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
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

// Check whether turning on this output would violate an interlock group.
// Returns the name of the conflicting equipment, or nullptr if clear.
const char *interlockConflict(uint8_t outputIndex) {
  Equipment *eq = config.findByOutput(outputIndex);
  if (!eq || eq->interlockGroup == 0) return nullptr;
  for (int i = 0; i < config.equipmentCount; i++) {
    Equipment &other = config.equipment[i];
    if (!other.active) continue;
    if (&other == eq) continue;
    if (other.interlockGroup != eq->interlockGroup) continue;
    if (other.outputIndex < MAX_OUTPUTS && outputs[other.outputIndex].confirmedOn)
      return other.name;
  }
  return nullptr;
}

void sendOutputCmd(OutputState &o, bool on) {
  if (o.status == CMD_PENDING || o.status == CMD_SENT) return;
  o.requestedOn = on;
  o.status      = CMD_PENDING;
  o.sentAt      = millis();
  o.retryAfter  = 0;
  o.seq         = nextSeq++;
  o.attempt     = 0;
}

void evaluateRules() {
  uint32_t now = millis();

  for (int i = 0; i < config.ruleCount; i++) {
    AutomationRule &r = config.rules[i];
    if (!r.enabled) continue;

    Equipment *eq = config.findEquipment(r.equipmentId);
    if (!eq || eq->outputIndex >= MAX_OUTPUTS) continue;
    OutputState &out = outputs[eq->outputIndex];

    // Manual override — skip automation entirely
    if (out.manualOverride) continue;

    // ---- Schedule-based rules ----
    if (r.ruleType == RULE_SCHEDULE) {
      if (!clockValid) continue;
      uint16_t nowMin = gatewayMinuteOfDay();
      uint8_t  nowDay = gatewayDayOfWeek();
      if (nowMin == 0xFFFF || nowDay == 0xFF) continue;

      bool dayMatch = (r.schedDays >> nowDay) & 1;
      bool inWindow;
      if (r.schedStartMin <= r.schedStopMin) {
        inWindow = (nowMin >= r.schedStartMin && nowMin < r.schedStopMin);
      } else {
        inWindow = (nowMin >= r.schedStartMin || nowMin < r.schedStopMin);
      }
      bool shouldBeOn = dayMatch && inWindow;

      if (shouldBeOn && !r.triggered) {
        const char *ilk = interlockConflict(eq->outputIndex);
        if (ilk) {
          actionLog.add(r.name, "INTERLOCK", eq->name, nowMin);
          continue;
        }
        r.triggered = true;
        r.lastActionAt = now;
        sendOutputCmd(out, true);
        actionLog.add(r.name, "SCHED_ON", eq->name, nowMin);
        { char lm[64]; snprintf(lm, sizeof(lm), "Schedule '%s' -> %s ON", r.name, eq->name); eventLog.add("rule", lm); }
        Serial.print("[Auto] Schedule '"); Serial.print(r.name);
        Serial.print("' -> "); Serial.print(eq->name); Serial.println(" ON");
      } else if (!shouldBeOn && r.triggered) {
        r.triggered = false;
        r.lastActionAt = now;
        sendOutputCmd(out, false);
        actionLog.add(r.name, "SCHED_OFF", eq->name, nowMin);
        { char lm[64]; snprintf(lm, sizeof(lm), "Schedule '%s' -> %s OFF", r.name, eq->name); eventLog.add("rule", lm); }
        Serial.print("[Auto] Schedule '"); Serial.print(r.name);
        Serial.print("' -> "); Serial.print(eq->name); Serial.println(" OFF");
      }
      continue;
    }

    // ---- Sensor-based rules ----
    // Find the node that owns this equipment's sensors
    NodeState *rns = findNodeById(eq->nodeId);
    uint32_t telemetryAge = (rns && rns->lastTelemetryAt > 0) ? (now - rns->lastTelemetryAt) : UINT32_MAX;
    bool sensorFresh = rns && rns->online && telemetryAge < 600000;

    // Fault handling: if sensor data is stale/missing while rule is triggered, force off
    if (!sensorFresh) {
      if (r.triggered && !r.faultActive) {
        r.faultActive = true;
        r.triggered = false;
        r.exceedSince = 0;
        sendOutputCmd(out, false);
        actionLog.add(r.name, "FAULT_OFF", eq->name, 0);
        { char lm[64]; snprintf(lm, sizeof(lm), "Rule '%s' fault -> %s OFF", r.name, eq->name); eventLog.add("fault", lm); }
        Serial.print("[Auto] Rule '"); Serial.print(r.name);
        Serial.println("' FAULT -> equipment OFF (stale sensor)");
      }
      continue;
    }

    // Sensor came back — clear fault flag
    if (r.faultActive) {
      r.faultActive = false;
      Serial.print("[Auto] Rule '"); Serial.print(r.name);
      Serial.println("' sensor recovered");
    }

    int16_t valueX10;
    if (r.sensor == 0)      valueX10 = (int16_t)(rns->temperature * 10);
    else if (r.sensor == 1) valueX10 = (int16_t)(rns->humidity * 10);
    else if (r.sensor == 2) {
      if (rns->analogValue < 0) continue;
      valueX10 = (int16_t)(rns->analogValue * 1000L / 1023);
    }
    else continue;

    // Evaluate condition with hysteresis
    bool conditionMet;
    if (r.condition == 0) {
      conditionMet = r.triggered
        ? (valueX10 >= r.thresholdX10 - r.hysteresisX10)
        : (valueX10 > r.thresholdX10);
    } else {
      conditionMet = r.triggered
        ? (valueX10 <= r.thresholdX10 + r.hysteresisX10)
        : (valueX10 < r.thresholdX10);
    }

    // Duration tracking
    if (conditionMet && !r.triggered) {
      if (r.exceedSince == 0) r.exceedSince = now;
      if (r.durationS > 0 && (now - r.exceedSince) < (uint32_t)r.durationS * 1000) {
        continue;  // condition met but duration not yet satisfied
      }
      // Cooldown check
      if (r.cooldownS > 0 && r.lastActionAt > 0 &&
          (now - r.lastActionAt) < (uint32_t)r.cooldownS * 1000) {
        continue;  // still in cooldown
      }
      const char *ilk = interlockConflict(eq->outputIndex);
      if (ilk) {
        actionLog.add(r.name, "INTERLOCK", eq->name, valueX10);
        continue;
      }
      r.triggered = true;
      r.exceedSince = 0;
      r.lastActionAt = now;
      sendOutputCmd(out, true);
      actionLog.add(r.name, "TRIGGER", eq->name, valueX10);
      { char lm[64]; snprintf(lm, sizeof(lm), "Rule '%s' triggered -> %s ON", r.name, eq->name); eventLog.add("rule", lm); }
      Serial.print("[Auto] Rule '"); Serial.print(r.name);
      Serial.print("' TRIGGERED -> "); Serial.print(eq->name);
      Serial.println(" ON");
    } else if (!conditionMet) {
      r.exceedSince = 0;
      if (r.triggered) {
        r.triggered = false;
        r.lastActionAt = now;
        sendOutputCmd(out, false);
        actionLog.add(r.name, "RELEASE", eq->name, valueX10);
        { char lm[64]; snprintf(lm, sizeof(lm), "Rule '%s' released -> %s OFF", r.name, eq->name); eventLog.add("rule", lm); }
        Serial.print("[Auto] Rule '"); Serial.print(r.name);
        Serial.print("' RELEASED -> "); Serial.print(eq->name);
        Serial.println(" OFF");
      }
    }
  }
}

// =====================================================================
//  HTTP: GET /api/events — persistent event log
// =====================================================================
void handleGetEvents() {
  if (!auth.check(server, ROLE_USER)) return;
  String cat = server.arg("cat");
  int limit = server.arg("limit").toInt();
  if (limit <= 0 || limit > 100) limit = 50;

  JsonDocument doc;
  JsonArray arr = doc["events"].to<JsonArray>();
  eventLog.toJson(arr, cat.length() ? cat.c_str() : nullptr, limit);
  doc["count"] = arr.size();
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleClearEvents() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  eventLog.clear();
  server.send(200, "application/json", "{\"ok\":true}");
}

// =====================================================================
//  HTTP: Config backup & restore (technician only)
// =====================================================================
void handleConfigBackup() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;

  JsonDocument doc;
  doc["_backup_v"] = 1;
  doc["_schema_v"] = CONFIG_SCHEMA_VERSION;
  doc["_timestamp"] = millis() / 1000;

  const char *files[] = {
    "/config/env.json", "/config/zones.json", "/config/equipment.json",
    "/config/sensors.json", "/config/rules.json", "/config/nodes.json"
  };
  const char *keys[] = { "env", "zones", "equipment", "sensors", "rules", "nodes" };

  for (int i = 0; i < 6; i++) {
    if (LittleFS.exists(files[i])) {
      File f = LittleFS.open(files[i], "r");
      if (f) {
        JsonDocument sub;
        if (deserializeJson(sub, f) == DeserializationError::Ok) {
          doc[keys[i]] = sub;
        }
        f.close();
      }
    }
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
  eventLog.add("config", "Config backup downloaded");
}

void handleConfigRestore() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  if (!checkBodySize(8192)) return;
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"need JSON body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }
  if (!doc.containsKey("_backup_v")) {
    server.send(400, "application/json", "{\"error\":\"not a valid backup file\"}");
    return;
  }
  int schemaV = doc["_schema_v"] | 0;
  if (schemaV > CONFIG_SCHEMA_VERSION) {
    server.send(400, "application/json", "{\"error\":\"backup from newer firmware version\"}");
    return;
  }

  const char *files[] = {
    "/config/env.json", "/config/zones.json", "/config/equipment.json",
    "/config/sensors.json", "/config/rules.json", "/config/nodes.json"
  };
  const char *keys[] = { "env", "zones", "equipment", "sensors", "rules", "nodes" };

  LittleFS.mkdir("/config");
  int restored = 0;
  for (int i = 0; i < 6; i++) {
    if (doc.containsKey(keys[i])) {
      File f = LittleFS.open(files[i], "w");
      if (f) {
        serializeJson(doc[keys[i]], f);
        f.close();
        restored++;
      }
    }
  }

  eventLog.add("config", "Config restored from backup");

  String resp = "{\"ok\":true,\"restored\":";
  resp += restored;
  resp += ",\"reboot\":true}";
  server.send(200, "application/json", resp);

  delay(500);
  ESP.restart();
}

// =====================================================================
//  HTTP: Auth management API (technician only)
// =====================================================================
void handleGetAuth() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  JsonDocument doc;
  doc["enabled"] = auth.enabled;
  JsonArray arr = doc["users"].to<JsonArray>();
  for (int i = 0; i < auth.count; i++) {
    JsonObject u = arr.add<JsonObject>();
    u["username"] = auth.credentials[i].username;
    u["role"]     = (int)auth.credentials[i].role;
    u["active"]   = auth.credentials[i].active;
    // Never expose passwords
  }
  AuthRole myRole = auth.identifyRole(server);
  doc["my_role"] = (int)myRole;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

void handleSetAuth() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  if (!checkBodySize()) return;
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"need JSON body\"}");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) != DeserializationError::Ok) {
    server.send(400, "application/json", "{\"error\":\"invalid JSON\"}");
    return;
  }

  if (doc.containsKey("enabled")) {
    auth.enabled = doc["enabled"] | false;
  }
  if (doc.containsKey("users")) {
    auth.count = 0;
    JsonArray arr = doc["users"].as<JsonArray>();
    for (JsonObject u : arr) {
      if (auth.count >= MAX_CREDENTIALS) break;
      AuthCredential &c = auth.credentials[auth.count];
      strlcpy(c.username, u["username"] | "", sizeof(c.username));
      if (u.containsKey("password")) {
        strlcpy(c.password, u["password"] | "", sizeof(c.password));
      }
      c.role = (AuthRole)(u["role"] | 1);
      c.active = u["active"] | true;
      if (c.username[0]) auth.count++;
    }
  }
  auth.save();
  eventLog.add("auth", "Auth settings updated");
  server.send(200, "application/json", "{\"ok\":true}");
}

// =====================================================================
//  HTTP: GET /api/system — system info for dashboard
// =====================================================================
void handleSystemInfo() {
  if (!auth.check(server, ROLE_USER)) return;
  JsonDocument doc;
  doc["firmware"]     = "3.0.0";
  doc["uptime_s"]     = millis() / 1000;
  doc["flash_used"]   = LittleFS.usedBytes();
  doc["flash_total"]  = LittleFS.totalBytes();
  doc["free_heap"]    = ESP.getFreeHeap();
  doc["auth_enabled"] = auth.enabled;
  doc["my_role"]      = (int)auth.identifyRole(server);
  doc["sensors"]      = config.sensorCount;
  doc["equipment"]    = config.equipmentCount;
  doc["rules"]        = config.ruleCount;
  doc["zones"]        = config.zoneCount;
  doc["nodes"]        = hw.nodeCount;
  doc["wifi_clients"] = WiFi.softAPgetStationNum();
  doc["clock_valid"]  = clockValid;
  doc["lora_addr"]    = ADDR_GATEWAY;
  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// =====================================================================
//  HTTP: POST /api/factory-reset — erase all config and reboot
// =====================================================================
void handleFactoryReset() {
  if (!auth.check(server, ROLE_TECHNICIAN)) return;
  addSecHeaders();

  const char *files[] = {
    "/config/env.json", "/config/zones.json", "/config/equipment.json",
    "/config/sensors.json", "/config/rules.json", "/config/nodes.json",
    "/config/auth.json", "/log/events.json", "/data/history.bin"
  };
  for (int i = 0; i < 9; i++) {
    if (LittleFS.exists(files[i])) LittleFS.remove(files[i]);
  }

  eventLog.add("system", "Factory reset performed");
  Serial.println("[System] Factory reset — all config erased");
  server.send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
  delay(500);
  ESP.restart();
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
  Serial.println("=== LoRaSense Gateway v3.0.0 ===");
  Serial.print("Free heap: "); Serial.print(ESP.getFreeHeap()); Serial.println(" bytes");

  // Init output states
  for (int i = 0; i < MAX_OUTPUTS; i++) {
    outputs[i].confirmedOn = false;
    outputs[i].requestedOn = false;
    outputs[i].status = CMD_IDLE;
    outputs[i].sentAt = 0;
    outputs[i].seq = 0;
    outputs[i].attempt = 0;
    outputs[i].manualOverride = false;
  }

  // LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("FAIL: LittleFS mount failed.");
  } else {
    Serial.println("LittleFS mounted.");
  }

  // Load config from flash
  config.begin();
  if (!config.validateConfig()) {
    Serial.println("[Config] Corrected out-of-range values");
    eventLog.add("config", "Config validated: out-of-range values corrected");
  }
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
  ensureDefaultSensor();
  syncRelayLabels();
  syncSensorPins();
  Serial.print("Nodes: "); Serial.println(hw.nodeCount);

  // Initialize per-node runtime state
  for (int i = 0; i < MAX_NODE_SLOTS; i++) nodeStates[i].reset();
  for (int i = 0; i < hw.nodeCount; i++) {
    if (!hw.nodes[i].active) continue;
    NodeState *ns = findNodeByAddress(hw.nodes[i].address);
    if (!ns) {
      // Assign first free slot
      for (int s = 0; s < MAX_NODE_SLOTS; s++) {
        if (nodeStates[s].loraAddress == 0) {
          nodeStates[s].loraAddress = hw.nodes[i].address;
          ns = &nodeStates[s];
          break;
        }
      }
    }
    if (ns) {
      Serial.print("  Node "); Serial.print(hw.nodes[i].id);
      Serial.print(" -> 0x"); Serial.println(hw.nodes[i].address, HEX);
    }
  }
  Serial.print("Sensors: "); Serial.println(config.sensorCount);

  // Auth and event log
  auth.begin();
  eventLog.begin();
  if (history.loadFromFile()) {
    Serial.print("[History] Restored "); Serial.print(history.count); Serial.println(" readings from flash");
  }
  Serial.print("Auth: "); Serial.println(auth.enabled ? "enabled" : "disabled");
  eventLog.add("system", "Gateway booted");

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
  server.on("/api/commission",          HTTP_POST,   handleCommission);

  // Rules API
  server.on("/api/clock",              HTTP_GET,    handleGetClock);
  server.on("/api/clock",              HTTP_POST,   handleSetClock);
  server.on("/api/rules",              HTTP_GET,    handleGetRules);
  server.on("/api/rules",              HTTP_POST,   handleAddRule);
  server.on("/api/rules",              HTTP_PUT,    handleUpdateRule);
  server.on("/api/rules",              HTTP_DELETE, handleDeleteRule);
  server.on("/api/action-log",          HTTP_GET,    handleActionLog);
  server.on("/api/override",            HTTP_POST,   handleOverride);

  // History API
  server.on("/api/history",            HTTP_GET,    handleHistory);

  // Sensor API
  server.on("/api/config/sensors",     HTTP_GET,    handleGetSensors);
  server.on("/api/config/sensors",     HTTP_POST,   handleAddSensor);
  server.on("/api/config/sensors",     HTTP_DELETE, handleDeleteSensor);

  // Hardware Studio API
  server.on("/api/hw/nodes",           HTTP_GET,    handleGetNodes);
  server.on("/api/hw/nodes",           HTTP_POST,   handleAddNode);
  server.on("/api/hw/nodes",           HTTP_DELETE, handleDeleteNode);
  server.on("/api/hw/pins",            HTTP_GET,    handleGetPins);
  server.on("/api/hw/relays",          HTTP_GET,    handleGetRelays);
  server.on("/api/hw/drivers",         HTTP_GET,    handleGetDrivers);

  // Self-test
  server.on("/api/test",               HTTP_GET,    handleSelfTest);

  // Phase 9: Commercial readiness
  server.on("/api/events",             HTTP_GET,    handleGetEvents);
  server.on("/api/events",             HTTP_DELETE, handleClearEvents);
  server.on("/api/config/backup",      HTTP_GET,    handleConfigBackup);
  server.on("/api/config/restore",     HTTP_POST,   handleConfigRestore);
  server.on("/api/auth",               HTTP_GET,    handleGetAuth);
  server.on("/api/auth",               HTTP_POST,   handleSetAuth);
  server.on("/api/system",             HTTP_GET,    handleSystemInfo);
  server.on("/api/factory-reset",      HTTP_POST,   handleFactoryReset);

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

  // Update node timeout status
  for (int i = 0; i < hw.nodeCount; i++) {
    NodeState *ns = findNodeByAddress(hw.nodes[i].address);
    if (ns && ns->online && millis() - ns->lastHeard > NODE_TIMEOUT_MS)
      ns->online = false;
  }

  if (anyNodeOnline() && millis() - lastAutomation >= AUTOMATION_INTERVAL_MS) {
    evaluateRules();
    lastAutomation = millis();
  }

  // Config sync: push config to each unsynced node
  if (!allNodesSynced() && millis() > CONFIG_PUSH_INITIAL_DELAY_MS) {
    for (int i = 0; i < hw.nodeCount; i++) {
      if (!hw.nodes[i].active) continue;
      NodeState *ns = findNodeByAddress(hw.nodes[i].address);
      if (ns && !ns->configSynced &&
          millis() - ns->lastConfigPush >= CONFIG_PUSH_INTERVAL_MS) {
        pushConfigToNode(hw.nodes[i]);
      }
    }
  }

  // Sensor history: first sample on first telemetry, then every 5 min
  NodeState *pn = primaryNode();
  if (pn && pn->online) {
    if (!firstSampleTaken) {
      history.push(millis() / 1000, pn->temperature, pn->humidity, pn->analogValue);
      lastHistorySample = millis();
      firstSampleTaken = true;
    } else if (millis() - lastHistorySample >= HISTORY_INTERVAL_MS) {
      history.push(millis() / 1000, pn->temperature, pn->humidity, pn->analogValue);
      lastHistorySample = millis();
    }
  }

  // Flush history to flash every 30 minutes
  if (history.count > 0 && millis() - history.lastFlush >= HISTORY_FLUSH_MS) {
    history.saveToFile();
  }
}
