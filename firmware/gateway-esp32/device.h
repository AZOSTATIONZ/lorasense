// Device runtime state — tracks the live command lifecycle for each
// equipment record that has a relay output assigned.

#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

enum CmdStatus { CMD_IDLE, CMD_PENDING, CMD_SENT, CMD_CONFIRMED, CMD_FAILED };

inline const char *statusName(CmdStatus s) {
  switch (s) {
    case CMD_PENDING:   return "PENDING";
    case CMD_SENT:      return "SENT";
    case CMD_CONFIRMED: return "CONFIRMED";
    case CMD_FAILED:    return "FAILED";
    default:            return "IDLE";
  }
}

// Runtime state for each relay output channel (indexed by output 0-3)
const int MAX_OUTPUTS = 4;

struct OutputState {
  bool      confirmedOn;
  bool      requestedOn;
  CmdStatus status;
  uint32_t  sentAt;
  uint8_t   seq;
  uint8_t   attempt;
  bool      manualOverride;  // when true, automation rules skip this output
};

// --- Action log ring buffer ---
const int ACTION_LOG_SIZE = 32;

struct ActionLogEntry {
  uint32_t timestamp;    // millis()
  char     ruleName[32];
  char     action[16];   // "TRIGGER", "RELEASE", "FAULT_OFF", "COOLDOWN", "OVERRIDE"
  char     equipment[32];
  int16_t  valueX10;
};

struct ActionLog {
  ActionLogEntry entries[ACTION_LOG_SIZE];
  int head;
  int count;

  ActionLog() : head(0), count(0) {}

  void add(const char *rule, const char *act, const char *equip, int16_t val) {
    ActionLogEntry &e = entries[head];
    e.timestamp = millis();
    strlcpy(e.ruleName, rule, sizeof(e.ruleName));
    strlcpy(e.action, act, sizeof(e.action));
    strlcpy(e.equipment, equip, sizeof(e.equipment));
    e.valueX10 = val;
    head = (head + 1) % ACTION_LOG_SIZE;
    if (count < ACTION_LOG_SIZE) count++;
  }
};

// --- Persistent event log (survives reboots) ---
const char *EVENT_LOG_FILE = "/log/events.json";
const int EVENT_LOG_MAX = 100;

struct EventLogEntry {
  uint32_t uptimeSec;
  char category[12];  // "rule", "relay", "config", "auth", "fault", "system"
  char message[64];
};

struct EventLog {
  void begin() {
    LittleFS.mkdir("/log");
  }

  void add(const char *category, const char *message) {
    // Read existing entries
    JsonDocument doc;
    if (LittleFS.exists(EVENT_LOG_FILE)) {
      File f = LittleFS.open(EVENT_LOG_FILE, "r");
      if (f) { deserializeJson(doc, f); f.close(); }
    }

    JsonArray arr = doc["events"].is<JsonArray>() ? doc["events"].as<JsonArray>() : doc["events"].to<JsonArray>();

    // Trim to keep under max
    while (arr.size() >= EVENT_LOG_MAX) arr.remove(0);

    JsonObject entry = arr.add<JsonObject>();
    entry["t"] = millis() / 1000;
    entry["cat"] = category;
    entry["msg"] = message;

    File f = LittleFS.open(EVENT_LOG_FILE, "w");
    if (f) { serializeJson(doc, f); f.close(); }
  }

  void toJson(JsonArray &out, const char *filterCat = nullptr, int limit = 50) {
    if (!LittleFS.exists(EVENT_LOG_FILE)) return;
    File f = LittleFS.open(EVENT_LOG_FILE, "r");
    if (!f) return;

    JsonDocument doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return; }
    f.close();

    JsonArray arr = doc["events"].as<JsonArray>();
    int start = 0;
    int total = arr.size();
    if (filterCat) {
      // Count matching
      int matching = 0;
      for (int i = total - 1; i >= 0 && matching < limit; i--) {
        JsonObject e = arr[i];
        if (strcmp(e["cat"] | "", filterCat) == 0) matching++;
      }
    }

    int added = 0;
    for (int i = total - 1; i >= 0 && added < limit; i--) {
      JsonObject e = arr[i];
      if (filterCat && strcmp(e["cat"] | "", filterCat) != 0) continue;
      JsonObject o = out.add<JsonObject>();
      o["t"]   = e["t"];
      o["cat"] = e["cat"];
      o["msg"] = e["msg"];
      added++;
    }
  }

  void clear() {
    if (LittleFS.exists(EVENT_LOG_FILE)) LittleFS.remove(EVENT_LOG_FILE);
  }
};
