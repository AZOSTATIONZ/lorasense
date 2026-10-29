#pragma once
#include <Arduino.h>
#include <LittleFS.h>

struct SensorReading {
  uint32_t uptimeSec;
  int16_t  tempX10;
  int16_t  humX10;
  int16_t  analogX10;
};

const int HISTORY_MAX = 288;  // 24h at 5-min intervals
const char *HISTORY_FILE = "/data/history.bin";
const uint8_t HISTORY_VERSION = 1;

class SensorHistory {
public:
  SensorReading buf[HISTORY_MAX];
  int head;
  int count;
  uint32_t lastFlush;

  SensorHistory() : head(0), count(0), lastFlush(0) {}

  void push(uint32_t uptimeSec, float temp, float hum, int16_t analogVal = -1) {
    buf[head].uptimeSec = uptimeSec;
    buf[head].tempX10   = (int16_t)(temp * 10);
    buf[head].humX10    = (int16_t)(hum * 10);
    buf[head].analogX10 = analogVal;
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

  bool saveToFile() {
    LittleFS.mkdir("/data");
    File f = LittleFS.open(HISTORY_FILE, "w");
    if (!f) return false;
    uint8_t header[4] = { 'H', 'S', HISTORY_VERSION, sizeof(SensorReading) };
    f.write(header, 4);
    int16_t meta[2] = { (int16_t)head, (int16_t)count };
    f.write((uint8_t *)meta, 4);
    f.write((uint8_t *)buf, sizeof(buf));
    f.close();
    lastFlush = millis();
    return true;
  }

  bool loadFromFile() {
    if (!LittleFS.exists(HISTORY_FILE)) return false;
    File f = LittleFS.open(HISTORY_FILE, "r");
    if (!f) return false;
    uint8_t header[4];
    if (f.read(header, 4) != 4) { f.close(); return false; }
    if (header[0] != 'H' || header[1] != 'S' ||
        header[2] != HISTORY_VERSION ||
        header[3] != sizeof(SensorReading)) {
      f.close();
      return false;
    }
    int16_t meta[2];
    if (f.read((uint8_t *)meta, 4) != 4) { f.close(); return false; }
    if (f.read((uint8_t *)buf, sizeof(buf)) != sizeof(buf)) { f.close(); return false; }
    f.close();
    head  = constrain(meta[0], 0, HISTORY_MAX - 1);
    count = constrain(meta[1], 0, HISTORY_MAX);
    lastFlush = millis();
    return true;
  }
};
