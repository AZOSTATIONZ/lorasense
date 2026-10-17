// Device model (later this comes from the device registry, Phase 6).
// Kept in its own header so the Arduino IDE sees these types before
// it auto-generates function prototypes in the .ino file.

#pragma once
#include <Arduino.h>

// Where a command is in its life. Prefixed CMD_ because the ESP32 core
// already uses plain names like PENDING.
enum CmdStatus { CMD_IDLE, CMD_PENDING, CMD_CONFIRMED, CMD_FAILED };

inline const char *statusName(CmdStatus s) {
  switch (s) {
    case CMD_PENDING:   return "PENDING";
    case CMD_CONFIRMED: return "CONFIRMED";
    case CMD_FAILED:    return "FAILED";
    default:            return "IDLE";
  }
}

struct Device {
  const char *id;          // used in the API, e.g. "light1"
  const char *name;        // shown on the dashboard
  bool        confirmedOn; // the state the NODE has confirmed - the only truth the UI shows
  bool        requestedOn; // what the user asked for
  CmdStatus   status;      // where the last command is in its life
  uint32_t    sentAt;      // millis() when the command was accepted
  uint8_t     seq;         // sequence number for matching the ACK
};
