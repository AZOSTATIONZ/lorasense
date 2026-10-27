#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Config schema version — bump when the JSON format changes
const uint8_t CONFIG_SCHEMA_VERSION = 1;

// Maximum limits for the config system
const int MAX_EQUIPMENT = 8;
const int MAX_SENSORS   = 8;
const int MAX_ZONES     = 8;
const int MAX_RULES     = 8;
const int ID_LEN        = 16;
const int NAME_LEN      = 32;

// Commissioning status — tracks installation completeness
enum CommissionStatus : uint8_t {
  CS_DRAFT        = 0,  // wizard started but not finished
  CS_CONFIGURED   = 1,  // wizard complete, system configured
  CS_TESTED       = 2,  // technician has verified operation
  CS_COMMISSIONED = 3,  // installation signed off
};

// --- Environment ---
struct Environment {
  char name[NAME_LEN];
  char type[16];       // home, farm, greenhouse, industrial, lab, custom
  bool configured;
  CommissionStatus commissionStatus;
  uint8_t relayCount;  // configured relay channel count (1-8, default 4)
};

// --- Zone ---
struct Zone {
  char id[ID_LEN];
  char name[NAME_LEN];
  uint8_t order;
};

// --- Equipment record ---
struct Equipment {
  char    id[ID_LEN];
  char    name[NAME_LEN];
  char    category[16];    // lighting, fan, pump, socket, irrigation, heating, other
  char    zoneId[ID_LEN];
  char    nodeId[ID_LEN];  // controller node this equipment belongs to
  uint8_t outputIndex;     // relay channel 0-3 (or 0xFF = unassigned)
  bool    active;          // whether this record is in use
};

// Measurement quality — how trustworthy is the current reading?
enum MeasureQuality : uint8_t {
  MQ_FRESH   = 0,  // received within expected interval
  MQ_STALE   = 1,  // older than 2× the reporting interval
  MQ_INVALID = 2,  // sensor reports out-of-range or NaN
  MQ_MISSING = 3,  // no reading ever received
};

// --- Sensor instance — a configured sensor in this installation ---
struct SensorInstance {
  char    id[ID_LEN];
  char    name[NAME_LEN];
  char    label[NAME_LEN]; // user-facing label, e.g. "Soil probe #1"
  uint8_t sensorType;      // SensorType enum value
  char    nodeId[ID_LEN];  // controller node
  uint8_t pin;             // assigned pin (0xFF = bus device, no pin)
  char    zoneId[ID_LEN];
  int16_t offsetX10;       // calibration offset × 10 (added to raw reading)
  uint16_t scaleX100;      // calibration scale × 100 (100 = 1.00, no scaling)
  bool    enabled;
  bool    active;          // whether this record slot is in use
};

// --- Automation rule ---
const uint8_t RULE_SENSOR   = 0;
const uint8_t RULE_SCHEDULE = 1;

struct AutomationRule {
  char    id[ID_LEN];
  char    name[NAME_LEN];
  uint8_t ruleType;        // RULE_SENSOR or RULE_SCHEDULE
  uint8_t sensor;          // 0 = temperature, 1 = humidity, 2 = analog
  uint8_t condition;       // 0 = above, 1 = below
  int16_t thresholdX10;    // e.g. 300 = 30.0°C
  int16_t hysteresisX10;   // e.g. 30 = 3.0°C
  char    equipmentId[ID_LEN];
  bool    enabled;
  uint16_t durationS;      // sensor must exceed threshold for this many seconds
  uint16_t cooldownS;      // minimum seconds between re-activations
  // Schedule fields (RULE_SCHEDULE only)
  uint16_t schedStartMin;  // start time in minutes from midnight (0-1439)
  uint16_t schedStopMin;   // stop time in minutes from midnight (0-1439)
  uint8_t  schedDays;      // bitmask: bit0=Sun .. bit6=Sat (0x7F=every day)
  // --- runtime state (not persisted) ---
  bool     triggered;
  uint32_t exceedSince;    // millis() when condition first became true (0 = not exceeding)
  uint32_t lastActionAt;   // millis() of last trigger/release action
  bool     faultActive;    // true if rule was disabled due to stale/missing sensor
};

// --- Config manager ---
class ConfigManager {
public:
  Environment env;
  Zone zones[MAX_ZONES];
  int zoneCount;
  Equipment equipment[MAX_EQUIPMENT];
  int equipmentCount;
  SensorInstance sensors[MAX_SENSORS];
  int sensorCount;
  AutomationRule rules[MAX_RULES];
  int ruleCount;

  ConfigManager() : zoneCount(0), equipmentCount(0), sensorCount(0), ruleCount(0) {
    memset(&env, 0, sizeof(env));
    env.relayCount = 4;
    memset(zones, 0, sizeof(zones));
    memset(equipment, 0, sizeof(equipment));
    memset(sensors, 0, sizeof(sensors));
    memset(rules, 0, sizeof(rules));
  }

  bool begin() {
    loadEnvironment();
    loadZones();
    loadEquipment();
    loadSensors();
    loadRules();
    return true;
  }

  // --- Environment ---
  void loadEnvironment() {
    File f = LittleFS.open("/config/env.json", "r");
    if (!f) {
      env.configured = false;
      return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, f) == DeserializationError::Ok) {
      strlcpy(env.name, doc["name"] | "My Home", NAME_LEN);
      strlcpy(env.type, doc["type"] | "home", sizeof(env.type));
      env.commissionStatus = (CommissionStatus)(doc["commission"] | (int)CS_DRAFT);
      env.relayCount = constrain(doc["relay_count"] | 4, 1, MAX_OUTPUTS);
      env.configured = true;
    }
    f.close();
  }

  bool saveEnvironment() {
    LittleFS.mkdir("/config");
    File f = LittleFS.open("/config/env.json", "w");
    if (!f) return false;
    JsonDocument doc;
    doc["_v"]   = CONFIG_SCHEMA_VERSION;
    doc["name"] = env.name;
    doc["type"] = env.type;
    doc["commission"] = (int)env.commissionStatus;
    doc["relay_count"] = env.relayCount;
    serializeJson(doc, f);
    f.close();
    env.configured = true;
    return true;
  }

  // --- Zones ---
  void loadZones() {
    zoneCount = 0;
    File f = LittleFS.open("/config/zones.json", "r");
    if (!f) return;
    JsonDocument doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return; }
    JsonArray arr = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc["items"].as<JsonArray>();
    for (JsonObject obj : arr) {
      if (zoneCount >= MAX_ZONES) break;
      Zone &z = zones[zoneCount];
      strlcpy(z.id, obj["id"] | "", ID_LEN);
      strlcpy(z.name, obj["name"] | "", NAME_LEN);
      z.order = obj["order"] | zoneCount;
      zoneCount++;
    }
    f.close();
  }

  bool saveZones() {
    LittleFS.mkdir("/config");
    File f = LittleFS.open("/config/zones.json", "w");
    if (!f) return false;
    JsonDocument doc;
    doc["_v"] = CONFIG_SCHEMA_VERSION;
    JsonArray arr = doc["items"].to<JsonArray>();
    for (int i = 0; i < zoneCount; i++) {
      JsonObject obj = arr.add<JsonObject>();
      obj["id"] = zones[i].id;
      obj["name"] = zones[i].name;
      obj["order"] = zones[i].order;
    }
    serializeJson(doc, f);
    f.close();
    return true;
  }

  Zone *findZone(const char *id) {
    for (int i = 0; i < zoneCount; i++)
      if (strcmp(zones[i].id, id) == 0) return &zones[i];
    return nullptr;
  }

  bool addZone(const char *id, const char *name) {
    if (zoneCount >= MAX_ZONES) return false;
    if (findZone(id)) return false;
    Zone &z = zones[zoneCount];
    strlcpy(z.id, id, ID_LEN);
    strlcpy(z.name, name, NAME_LEN);
    z.order = zoneCount;
    zoneCount++;
    return saveZones();
  }

  bool removeZone(const char *id) {
    for (int i = 0; i < zoneCount; i++) {
      if (strcmp(zones[i].id, id) == 0) {
        for (int j = i; j < zoneCount - 1; j++) zones[j] = zones[j + 1];
        zoneCount--;
        return saveZones();
      }
    }
    return false;
  }

  // --- Equipment ---
  void loadEquipment() {
    equipmentCount = 0;
    File f = LittleFS.open("/config/equipment.json", "r");
    if (!f) return;
    JsonDocument doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return; }
    JsonArray arr = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc["items"].as<JsonArray>();
    for (JsonObject obj : arr) {
      if (equipmentCount >= MAX_EQUIPMENT) break;
      Equipment &eq = equipment[equipmentCount];
      strlcpy(eq.id, obj["id"] | "", ID_LEN);
      strlcpy(eq.name, obj["name"] | "", NAME_LEN);
      strlcpy(eq.category, obj["category"] | "other", sizeof(eq.category));
      strlcpy(eq.zoneId, obj["zone_id"] | "", ID_LEN);
      strlcpy(eq.nodeId, obj["node_id"] | "mega-001", ID_LEN);
      eq.outputIndex = obj["output"] | 0xFF;
      eq.active = true;
      equipmentCount++;
    }
    f.close();
  }

  bool saveEquipment() {
    LittleFS.mkdir("/config");
    File f = LittleFS.open("/config/equipment.json", "w");
    if (!f) return false;
    JsonDocument doc;
    doc["_v"] = CONFIG_SCHEMA_VERSION;
    JsonArray arr = doc["items"].to<JsonArray>();
    for (int i = 0; i < equipmentCount; i++) {
      Equipment &eq = equipment[i];
      if (!eq.active) continue;
      JsonObject obj = arr.add<JsonObject>();
      obj["id"] = eq.id;
      obj["name"] = eq.name;
      obj["category"] = eq.category;
      obj["zone_id"] = eq.zoneId;
      obj["node_id"] = eq.nodeId;
      if (eq.outputIndex != 0xFF) obj["output"] = eq.outputIndex;
    }
    serializeJson(doc, f);
    f.close();
    return true;
  }

  Equipment *findEquipment(const char *id) {
    for (int i = 0; i < equipmentCount; i++)
      if (equipment[i].active && strcmp(equipment[i].id, id) == 0)
        return &equipment[i];
    return nullptr;
  }

  Equipment *findByOutput(uint8_t idx) {
    for (int i = 0; i < equipmentCount; i++)
      if (equipment[i].active && equipment[i].outputIndex == idx)
        return &equipment[i];
    return nullptr;
  }

  bool isOutputTaken(uint8_t idx, const char *excludeId = nullptr) {
    for (int i = 0; i < equipmentCount; i++) {
      if (!equipment[i].active) continue;
      if (equipment[i].outputIndex == idx) {
        if (excludeId && strcmp(equipment[i].id, excludeId) == 0) continue;
        return true;
      }
    }
    return false;
  }

  bool addEquipment(const char *id, const char *name, const char *category,
                    const char *zoneId, const char *nodeId, uint8_t output) {
    if (equipmentCount >= MAX_EQUIPMENT) return false;
    if (findEquipment(id)) return false;
    if (output != 0xFF && isOutputTaken(output)) return false;

    Equipment &eq = equipment[equipmentCount];
    strlcpy(eq.id, id, ID_LEN);
    strlcpy(eq.name, name, NAME_LEN);
    strlcpy(eq.category, category, sizeof(eq.category));
    strlcpy(eq.zoneId, zoneId, ID_LEN);
    strlcpy(eq.nodeId, nodeId, ID_LEN);
    eq.outputIndex = output;
    eq.active = true;
    equipmentCount++;
    return saveEquipment();
  }

  bool updateEquipment(const char *id, const char *name, const char *category,
                       const char *zoneId, const char *nodeId, uint8_t output) {
    Equipment *eq = findEquipment(id);
    if (!eq) return false;
    if (output != 0xFF && isOutputTaken(output, id)) return false;

    if (name[0])     strlcpy(eq->name, name, NAME_LEN);
    if (category[0]) strlcpy(eq->category, category, sizeof(eq->category));
    strlcpy(eq->zoneId, zoneId, ID_LEN);
    if (nodeId[0])   strlcpy(eq->nodeId, nodeId, ID_LEN);
    eq->outputIndex = output;
    return saveEquipment();
  }

  bool removeEquipment(const char *id) {
    for (int i = 0; i < equipmentCount; i++) {
      if (equipment[i].active && strcmp(equipment[i].id, id) == 0) {
        for (int j = i; j < equipmentCount - 1; j++) equipment[j] = equipment[j + 1];
        equipmentCount--;
        return saveEquipment();
      }
    }
    return false;
  }

  // --- Sensors ---
  void loadSensors() {
    sensorCount = 0;
    File f = LittleFS.open("/config/sensors.json", "r");
    if (!f) return;
    JsonDocument doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return; }
    JsonArray arr = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc["items"].as<JsonArray>();
    for (JsonObject obj : arr) {
      if (sensorCount >= MAX_SENSORS) break;
      SensorInstance &s = sensors[sensorCount];
      strlcpy(s.id, obj["id"] | "", ID_LEN);
      strlcpy(s.name, obj["name"] | "", NAME_LEN);
      strlcpy(s.label, obj["label"] | "", NAME_LEN);
      s.sensorType = obj["type"] | 0;
      strlcpy(s.nodeId, obj["node_id"] | "mega-001", ID_LEN);
      s.pin = obj["pin"] | 0xFF;
      strlcpy(s.zoneId, obj["zone_id"] | "", ID_LEN);
      s.offsetX10  = obj["cal_offset"] | (int16_t)0;
      s.scaleX100  = obj["cal_scale"]  | (uint16_t)100;
      s.enabled = obj["enabled"] | true;
      s.active = true;
      sensorCount++;
    }
    f.close();
  }

  bool saveSensors() {
    LittleFS.mkdir("/config");
    File f = LittleFS.open("/config/sensors.json", "w");
    if (!f) return false;
    JsonDocument doc;
    doc["_v"] = CONFIG_SCHEMA_VERSION;
    JsonArray arr = doc["items"].to<JsonArray>();
    for (int i = 0; i < sensorCount; i++) {
      SensorInstance &s = sensors[i];
      if (!s.active) continue;
      JsonObject obj = arr.add<JsonObject>();
      obj["id"]      = s.id;
      obj["name"]    = s.name;
      if (s.label[0]) obj["label"] = s.label;
      obj["type"]    = s.sensorType;
      obj["node_id"] = s.nodeId;
      if (s.pin != 0xFF) obj["pin"] = s.pin;
      obj["zone_id"] = s.zoneId;
      if (s.offsetX10 != 0)   obj["cal_offset"] = s.offsetX10;
      if (s.scaleX100 != 100) obj["cal_scale"]  = s.scaleX100;
      obj["enabled"] = s.enabled;
    }
    serializeJson(doc, f);
    f.close();
    return true;
  }

  SensorInstance *findSensor(const char *id) {
    for (int i = 0; i < sensorCount; i++)
      if (sensors[i].active && strcmp(sensors[i].id, id) == 0)
        return &sensors[i];
    return nullptr;
  }

  bool isSensorPinTaken(uint8_t pin, const char *excludeId = nullptr) {
    for (int i = 0; i < sensorCount; i++) {
      if (!sensors[i].active) continue;
      if (sensors[i].pin == pin) {
        if (excludeId && strcmp(sensors[i].id, excludeId) == 0) continue;
        return true;
      }
    }
    return false;
  }

  bool addSensor(const char *id, const char *name, uint8_t type,
                 const char *nodeId, uint8_t pin, const char *zoneId, bool enabled,
                 const char *label = "", int16_t offsetX10 = 0, uint16_t scaleX100 = 100) {
    if (sensorCount >= MAX_SENSORS) return false;
    if (findSensor(id)) return false;
    SensorInstance &s = sensors[sensorCount];
    strlcpy(s.id, id, ID_LEN);
    strlcpy(s.name, name, NAME_LEN);
    strlcpy(s.label, label, NAME_LEN);
    s.sensorType = type;
    strlcpy(s.nodeId, nodeId, ID_LEN);
    s.pin = pin;
    strlcpy(s.zoneId, zoneId, ID_LEN);
    s.offsetX10  = offsetX10;
    s.scaleX100  = scaleX100;
    s.enabled = enabled;
    s.active = true;
    sensorCount++;
    return saveSensors();
  }

  bool removeSensor(const char *id) {
    for (int i = 0; i < sensorCount; i++) {
      if (sensors[i].active && strcmp(sensors[i].id, id) == 0) {
        for (int j = i; j < sensorCount - 1; j++) sensors[j] = sensors[j + 1];
        sensorCount--;
        return saveSensors();
      }
    }
    return false;
  }

  // --- Automation rules ---
  void loadRules() {
    ruleCount = 0;
    File f = LittleFS.open("/config/rules.json", "r");
    if (!f) return;
    JsonDocument doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return; }
    JsonArray arr = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc["items"].as<JsonArray>();
    for (JsonObject obj : arr) {
      if (ruleCount >= MAX_RULES) break;
      AutomationRule &r = rules[ruleCount];
      strlcpy(r.id, obj["id"] | "", ID_LEN);
      strlcpy(r.name, obj["name"] | "", NAME_LEN);
      r.ruleType      = obj["rule_type"] | RULE_SENSOR;
      r.sensor        = obj["sensor"] | 0;
      r.condition     = obj["condition"] | 0;
      r.thresholdX10  = obj["threshold"] | 0;
      r.hysteresisX10 = obj["hysteresis"] | 0;
      strlcpy(r.equipmentId, obj["equipment"] | "", ID_LEN);
      r.enabled       = obj["enabled"] | true;
      r.durationS     = obj["duration_s"] | 0;
      r.cooldownS     = obj["cooldown_s"] | 0;
      r.schedStartMin = obj["sched_start"] | 0;
      r.schedStopMin  = obj["sched_stop"] | 0;
      r.schedDays     = obj["sched_days"] | 0x7F;
      r.triggered     = false;
      r.exceedSince   = 0;
      r.lastActionAt  = 0;
      r.faultActive   = false;
      ruleCount++;
    }
    f.close();
  }

  bool saveRules() {
    LittleFS.mkdir("/config");
    File f = LittleFS.open("/config/rules.json", "w");
    if (!f) return false;
    JsonDocument doc;
    doc["_v"] = CONFIG_SCHEMA_VERSION;
    JsonArray arr = doc["items"].to<JsonArray>();
    for (int i = 0; i < ruleCount; i++) {
      AutomationRule &r = rules[i];
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
      if (r.durationS)  obj["duration_s"]  = r.durationS;
      if (r.cooldownS)  obj["cooldown_s"]  = r.cooldownS;
      if (r.ruleType == RULE_SCHEDULE) {
        obj["sched_start"] = r.schedStartMin;
        obj["sched_stop"]  = r.schedStopMin;
        obj["sched_days"]  = r.schedDays;
      }
    }
    serializeJson(doc, f);
    f.close();
    return true;
  }

  AutomationRule *findRule(const char *id) {
    for (int i = 0; i < ruleCount; i++)
      if (strcmp(rules[i].id, id) == 0) return &rules[i];
    return nullptr;
  }

  bool addRule(const char *id, const char *name, uint8_t ruleType, uint8_t sensor, uint8_t condition,
               int16_t thresholdX10, int16_t hysteresisX10, const char *equipmentId, bool enabled,
               uint16_t durationS = 0, uint16_t cooldownS = 0,
               uint16_t schedStart = 0, uint16_t schedStop = 0, uint8_t schedDays = 0x7F) {
    if (ruleCount >= MAX_RULES) return false;
    if (findRule(id)) return false;
    AutomationRule &r = rules[ruleCount];
    strlcpy(r.id, id, ID_LEN);
    strlcpy(r.name, name, NAME_LEN);
    r.ruleType      = ruleType;
    r.sensor        = sensor;
    r.condition     = condition;
    r.thresholdX10  = thresholdX10;
    r.hysteresisX10 = hysteresisX10;
    strlcpy(r.equipmentId, equipmentId, ID_LEN);
    r.enabled       = enabled;
    r.durationS     = durationS;
    r.cooldownS     = cooldownS;
    r.schedStartMin = schedStart;
    r.schedStopMin  = schedStop;
    r.schedDays     = schedDays;
    r.triggered     = false;
    r.exceedSince   = 0;
    r.lastActionAt  = 0;
    r.faultActive   = false;
    ruleCount++;
    return saveRules();
  }

  bool updateRule(const char *id, const char *name, uint8_t ruleType, uint8_t sensor, uint8_t condition,
                  int16_t thresholdX10, int16_t hysteresisX10, const char *equipmentId, bool enabled,
                  uint16_t durationS = 0, uint16_t cooldownS = 0,
                  uint16_t schedStart = 0, uint16_t schedStop = 0, uint8_t schedDays = 0x7F) {
    AutomationRule *r = findRule(id);
    if (!r) return false;
    if (name[0]) strlcpy(r->name, name, NAME_LEN);
    r->ruleType      = ruleType;
    r->sensor        = sensor;
    r->condition     = condition;
    r->thresholdX10  = thresholdX10;
    r->hysteresisX10 = hysteresisX10;
    strlcpy(r->equipmentId, equipmentId, ID_LEN);
    r->enabled       = enabled;
    r->durationS     = durationS;
    r->cooldownS     = cooldownS;
    r->schedStartMin = schedStart;
    r->schedStopMin  = schedStop;
    r->schedDays     = schedDays;
    return saveRules();
  }

  bool removeRule(const char *id) {
    for (int i = 0; i < ruleCount; i++) {
      if (strcmp(rules[i].id, id) == 0) {
        for (int j = i; j < ruleCount - 1; j++) rules[j] = rules[j + 1];
        ruleCount--;
        return saveRules();
      }
    }
    return false;
  }

  // Create default config for first boot
  void createDefaults() {
    strlcpy(env.name, "My Home", NAME_LEN);
    strlcpy(env.type, "home", sizeof(env.type));
    env.configured = true;
    saveEnvironment();

    addZone("z_main", "Main");

    addEquipment("eq_1", "Living room light", "lighting", "z_main", "mega-001", 0);
    addEquipment("eq_2", "Bedroom light",     "lighting", "z_main", "mega-001", 1);
    addEquipment("eq_3", "Ceiling fan",        "fan",      "z_main", "mega-001", 2);
    addEquipment("eq_4", "Water pump",         "pump",     "z_main", "mega-001", 3);
  }
};
