#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Maximum limits for the config system
const int MAX_EQUIPMENT = 8;
const int MAX_ZONES     = 8;
const int ID_LEN        = 16;
const int NAME_LEN      = 32;

// --- Environment ---
struct Environment {
  char name[NAME_LEN];
  char type[16];       // home, farm, greenhouse, industrial, lab, custom
  bool configured;
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
  uint8_t outputIndex;     // relay channel 0-3 (or 0xFF = unassigned)
  bool    active;          // whether this record is in use
};

// --- Config manager ---
class ConfigManager {
public:
  Environment env;
  Zone zones[MAX_ZONES];
  int zoneCount;
  Equipment equipment[MAX_EQUIPMENT];
  int equipmentCount;

  ConfigManager() : zoneCount(0), equipmentCount(0) {
    memset(&env, 0, sizeof(env));
    memset(zones, 0, sizeof(zones));
    memset(equipment, 0, sizeof(equipment));
  }

  bool begin() {
    loadEnvironment();
    loadZones();
    loadEquipment();
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
      env.configured = true;
    }
    f.close();
  }

  bool saveEnvironment() {
    LittleFS.mkdir("/config");
    File f = LittleFS.open("/config/env.json", "w");
    if (!f) return false;
    JsonDocument doc;
    doc["name"] = env.name;
    doc["type"] = env.type;
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
    JsonArray arr = doc.as<JsonArray>();
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
    JsonArray arr = doc.to<JsonArray>();
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
    JsonArray arr = doc.as<JsonArray>();
    for (JsonObject obj : arr) {
      if (equipmentCount >= MAX_EQUIPMENT) break;
      Equipment &eq = equipment[equipmentCount];
      strlcpy(eq.id, obj["id"] | "", ID_LEN);
      strlcpy(eq.name, obj["name"] | "", NAME_LEN);
      strlcpy(eq.category, obj["category"] | "other", sizeof(eq.category));
      strlcpy(eq.zoneId, obj["zone_id"] | "", ID_LEN);
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
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < equipmentCount; i++) {
      Equipment &eq = equipment[i];
      if (!eq.active) continue;
      JsonObject obj = arr.add<JsonObject>();
      obj["id"] = eq.id;
      obj["name"] = eq.name;
      obj["category"] = eq.category;
      obj["zone_id"] = eq.zoneId;
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
                    const char *zoneId, uint8_t output) {
    if (equipmentCount >= MAX_EQUIPMENT) return false;
    if (findEquipment(id)) return false;
    if (output != 0xFF && isOutputTaken(output)) return false;

    Equipment &eq = equipment[equipmentCount];
    strlcpy(eq.id, id, ID_LEN);
    strlcpy(eq.name, name, NAME_LEN);
    strlcpy(eq.category, category, sizeof(eq.category));
    strlcpy(eq.zoneId, zoneId, ID_LEN);
    eq.outputIndex = output;
    eq.active = true;
    equipmentCount++;
    return saveEquipment();
  }

  bool updateEquipment(const char *id, const char *name, const char *category,
                       const char *zoneId, uint8_t output) {
    Equipment *eq = findEquipment(id);
    if (!eq) return false;
    if (output != 0xFF && isOutputTaken(output, id)) return false;

    if (name[0])     strlcpy(eq->name, name, NAME_LEN);
    if (category[0]) strlcpy(eq->category, category, sizeof(eq->category));
    strlcpy(eq->zoneId, zoneId, ID_LEN);
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

  // Create default config for first boot
  void createDefaults() {
    strlcpy(env.name, "My Home", NAME_LEN);
    strlcpy(env.type, "home", sizeof(env.type));
    env.configured = true;
    saveEnvironment();

    addZone("z_main", "Main");

    addEquipment("eq_1", "Living room light", "lighting", "z_main", 0);
    addEquipment("eq_2", "Bedroom light",     "lighting", "z_main", 1);
    addEquipment("eq_3", "Ceiling fan",        "fan",      "z_main", 2);
    addEquipment("eq_4", "Water pump",         "pump",     "z_main", 3);
  }
};
