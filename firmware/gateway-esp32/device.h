// Device runtime state — tracks the live command lifecycle for each
// equipment record that has a relay output assigned.

#pragma once
#include <Arduino.h>

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
