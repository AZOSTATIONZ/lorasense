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
};
