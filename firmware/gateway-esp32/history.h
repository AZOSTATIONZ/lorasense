#pragma once
#include <Arduino.h>

struct SensorReading {
  uint32_t uptimeSec;
  int16_t  tempX10;
  int16_t  humX10;
};

const int HISTORY_MAX = 288;  // 24h at 5-min intervals

class SensorHistory {
public:
  SensorReading buf[HISTORY_MAX];
  int head;
  int count;

  SensorHistory() : head(0), count(0) {}

  void push(uint32_t uptimeSec, float temp, float hum) {
    buf[head].uptimeSec = uptimeSec;
    buf[head].tempX10   = (int16_t)(temp * 10);
    buf[head].humX10    = (int16_t)(hum * 10);
    head = (head + 1) % HISTORY_MAX;
    if (count < HISTORY_MAX) count++;
  }

  int oldest() const {
    if (count < HISTORY_MAX) return 0;
    return head;
  }

  const SensorReading &at(int i) const {
    int idx = (oldest() + i) % HISTORY_MAX;
    return buf[idx];
  }
};
