// =====================================================================
//  Shared protocol header - used by BOTH the ESP32 and the Mega
//
//  This file defines the "language" the two boards speak over LoRa.
//  Every message (frame) has the same shape:
//
//    [ dst | src | type | seq | payload ... | crc_hi | crc_lo ]
//      1B    1B    1B     1B    0-56 bytes     1B       1B
//
//  dst/src  = who it's for / who sent it (addresses)
//  type     = what kind of message (command, ack, telemetry, etc.)
//  seq      = sequence number so we can match an ACK to its command
//  payload  = the actual data (device id + on/off, sensor readings, etc.)
//  crc      = CRC-16: a checksum so we can detect damaged messages
//
//  Both boards include this same file, so they always agree on the format.
// =====================================================================

#pragma once
#include <stdint.h>

// --- Addresses ---
// Each board has a unique address. 0xFF = broadcast (everyone listens).
const uint8_t ADDR_GATEWAY = 0x01;
const uint8_t ADDR_NODE1   = 0x02;
const uint8_t ADDR_BROADCAST = 0xFF;

// --- Message types ---
enum MsgType : uint8_t {
  MSG_CMD_SET   = 0x10,   // gateway -> node: "set device X to on/off"
  MSG_ACK       = 0x20,   // node -> gateway: "done, here's the actual state"
  MSG_TELEMETRY = 0x30,   // node -> gateway: sensor readings
  MSG_HEARTBEAT = 0x40,   // node -> gateway: "I'm still alive"
  MSG_KEEPALIVE = 0x50,   // gateway -> node: "I'm still here, don't fail-safe"
  MSG_PING      = 0xE0,   // test
  MSG_PONG      = 0xE1,   // test reply
};

// --- Frame header (4 bytes, always present) ---
struct FrameHeader {
  uint8_t dst;    // destination address
  uint8_t src;    // source address
  uint8_t type;   // MsgType
  uint8_t seq;    // sequence number (0-255, wraps)
};

// --- CMD_SET payload: which device, what state ---
struct __attribute__((packed)) CmdSetPayload {
  uint8_t deviceIndex;  // 0 = relay 1, 1 = relay 2, etc.
  uint8_t requestedOn;  // 1 = on, 0 = off
};

// --- ACK payload: what the node actually did ---
struct __attribute__((packed)) AckPayload {
  uint8_t deviceIndex;
  uint8_t actualOn;     // the REAL output state after the command
  uint8_t ok;           // 1 = success, 0 = rejected (bad index, interlock, etc.)
};

// --- Telemetry payload ---
struct __attribute__((packed)) TelemetryPayload {
  int16_t  temperatureX10;  // e.g. 245 = 24.5 °C
  int16_t  humidityX10;     // e.g. 655 = 65.5 %RH
  uint8_t  relayStates;     // bit 0 = relay 0, bit 1 = relay 1, etc.
};

// --- Frame size limits ---
const uint8_t FRAME_MAX = 64;
const uint8_t HEADER_SIZE = sizeof(FrameHeader);
const uint8_t CRC_SIZE = 2;

// =====================================================================
//  CRC-16/CCITT-FALSE - detects bit errors in the radio link
//
//  How it works in plain English:
//    Before sending, the transmitter runs this function over every byte
//    of the frame and appends the 2-byte result. The receiver runs the
//    same function and compares. If even one bit flipped in transit,
//    the numbers won't match, and the receiver drops the frame.
// =====================================================================
inline uint16_t crc16(const uint8_t *data, uint16_t len) {
  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < len; i++) {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
      else              crc <<= 1;
    }
  }
  return crc;
}

// --- Build a frame into a buffer. Returns total length including CRC. ---
inline uint8_t buildFrame(uint8_t *buf, uint8_t dst, uint8_t src,
                          MsgType type, uint8_t seq,
                          const void *payload, uint8_t payloadLen) {
  buf[0] = dst;
  buf[1] = src;
  buf[2] = (uint8_t)type;
  buf[3] = seq;
  if (payloadLen > 0 && payload != nullptr) {
    memcpy(buf + HEADER_SIZE, payload, payloadLen);
  }
  uint8_t bodyLen = HEADER_SIZE + payloadLen;
  uint16_t crc = crc16(buf, bodyLen);
  buf[bodyLen]     = (crc >> 8) & 0xFF;   // high byte first
  buf[bodyLen + 1] = crc & 0xFF;
  return bodyLen + CRC_SIZE;
}

// --- Validate a received frame. Returns true if CRC matches. ---
inline bool validateFrame(const uint8_t *buf, uint8_t len) {
  if (len < HEADER_SIZE + CRC_SIZE) return false;
  uint16_t received = ((uint16_t)buf[len - 2] << 8) | buf[len - 1];
  return crc16(buf, len - CRC_SIZE) == received;
}
