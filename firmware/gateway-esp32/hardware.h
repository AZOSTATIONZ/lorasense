#pragma once
#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// =====================================================================
//  Pin capability flags (bitfield)
// =====================================================================
const uint8_t CAP_DI  = 0x01;  // digital input
const uint8_t CAP_DO  = 0x02;  // digital output
const uint8_t CAP_AI  = 0x04;  // analog input
const uint8_t CAP_PWM = 0x08;  // PWM output
const uint8_t CAP_INT = 0x10;  // external interrupt

// =====================================================================
//  Pin state
// =====================================================================
enum PinState : uint8_t {
  PIN_AVAILABLE = 0,
  PIN_RESERVED_SYSTEM = 1,
  PIN_RESERVED_RADIO  = 2,
  PIN_ASSIGNED        = 3,
};

// =====================================================================
//  Static board profile — Arduino Mega 2560 pin capabilities
//  Stored in PROGMEM to save RAM.  Each entry: pin number + capability flags.
// =====================================================================
struct PinCap {
  uint8_t pin;
  uint8_t caps;
};

// Mega 2560: D0-D53 digital, A0-A15 analog (mapped as D54-D69)
// PWM on D2-D13, D44-D46
// Interrupts on D2, D3, D18, D19, D20, D21
const PinCap MEGA_PINS[] PROGMEM = {
  { 0, CAP_DI|CAP_DO},            // UART0 RX
  { 1, CAP_DI|CAP_DO},            // UART0 TX
  { 2, CAP_DI|CAP_DO|CAP_PWM|CAP_INT},
  { 3, CAP_DI|CAP_DO|CAP_PWM|CAP_INT},
  { 4, CAP_DI|CAP_DO|CAP_PWM},
  { 5, CAP_DI|CAP_DO|CAP_PWM},
  { 6, CAP_DI|CAP_DO|CAP_PWM},
  { 7, CAP_DI|CAP_DO|CAP_PWM},
  { 8, CAP_DI|CAP_DO|CAP_PWM},
  { 9, CAP_DI|CAP_DO|CAP_PWM},
  {10, CAP_DI|CAP_DO|CAP_PWM},
  {11, CAP_DI|CAP_DO|CAP_PWM},
  {12, CAP_DI|CAP_DO|CAP_PWM},
  {13, CAP_DI|CAP_DO|CAP_PWM},
  {14, CAP_DI|CAP_DO},            // UART3 TX
  {15, CAP_DI|CAP_DO},            // UART3 RX
  {16, CAP_DI|CAP_DO},            // UART2 TX
  {17, CAP_DI|CAP_DO},            // UART2 RX
  {18, CAP_DI|CAP_DO|CAP_INT},    // UART1 TX
  {19, CAP_DI|CAP_DO|CAP_INT},    // UART1 RX
  {20, CAP_DI|CAP_DO|CAP_INT},    // I2C SDA
  {21, CAP_DI|CAP_DO|CAP_INT},    // I2C SCL
  {22, CAP_DI|CAP_DO}, {23, CAP_DI|CAP_DO},
  {24, CAP_DI|CAP_DO}, {25, CAP_DI|CAP_DO},
  {26, CAP_DI|CAP_DO}, {27, CAP_DI|CAP_DO},
  {28, CAP_DI|CAP_DO}, {29, CAP_DI|CAP_DO},
  {30, CAP_DI|CAP_DO}, {31, CAP_DI|CAP_DO},
  {32, CAP_DI|CAP_DO}, {33, CAP_DI|CAP_DO},
  {34, CAP_DI|CAP_DO}, {35, CAP_DI|CAP_DO},
  {36, CAP_DI|CAP_DO}, {37, CAP_DI|CAP_DO},
  {38, CAP_DI|CAP_DO}, {39, CAP_DI|CAP_DO},
  {40, CAP_DI|CAP_DO}, {41, CAP_DI|CAP_DO},
  {42, CAP_DI|CAP_DO}, {43, CAP_DI|CAP_DO},
  {44, CAP_DI|CAP_DO|CAP_PWM},
  {45, CAP_DI|CAP_DO|CAP_PWM},
  {46, CAP_DI|CAP_DO|CAP_PWM},
  {47, CAP_DI|CAP_DO}, {48, CAP_DI|CAP_DO},
  {49, CAP_DI|CAP_DO},
  {50, CAP_DI|CAP_DO},            // SPI MISO
  {51, CAP_DI|CAP_DO},            // SPI MOSI
  {52, CAP_DI|CAP_DO},            // SPI SCK
  {53, CAP_DI|CAP_DO},            // SPI SS
  // A0-A15 = digital 54-69
  {54, CAP_DI|CAP_DO|CAP_AI}, {55, CAP_DI|CAP_DO|CAP_AI},
  {56, CAP_DI|CAP_DO|CAP_AI}, {57, CAP_DI|CAP_DO|CAP_AI},
  {58, CAP_DI|CAP_DO|CAP_AI}, {59, CAP_DI|CAP_DO|CAP_AI},
  {60, CAP_DI|CAP_DO|CAP_AI}, {61, CAP_DI|CAP_DO|CAP_AI},
  {62, CAP_DI|CAP_DO|CAP_AI}, {63, CAP_DI|CAP_DO|CAP_AI},
  {64, CAP_DI|CAP_DO|CAP_AI}, {65, CAP_DI|CAP_DO|CAP_AI},
  {66, CAP_DI|CAP_DO|CAP_AI}, {67, CAP_DI|CAP_DO|CAP_AI},
  {68, CAP_DI|CAP_DO|CAP_AI}, {69, CAP_DI|CAP_DO|CAP_AI},
};
const int MEGA_PIN_COUNT = sizeof(MEGA_PINS) / sizeof(MEGA_PINS[0]);

// =====================================================================
//  System/radio pin reservations — pins that cannot be reassigned
// =====================================================================
struct PinReservation {
  uint8_t pin;
  PinState state;
  const char *usedBy;
};

const PinReservation MEGA_RESERVATIONS[] PROGMEM = {
  {  0, PIN_RESERVED_SYSTEM, "UART0 RX"    },
  {  1, PIN_RESERVED_SYSTEM, "UART0 TX"    },
  {  2, PIN_RESERVED_RADIO,  "LoRa DIO0"   },
  {  9, PIN_RESERVED_RADIO,  "LoRa RST"    },
  { 13, PIN_RESERVED_SYSTEM, "Status LED"  },
  { 50, PIN_RESERVED_RADIO,  "LoRa MISO"   },
  { 51, PIN_RESERVED_RADIO,  "LoRa MOSI"   },
  { 52, PIN_RESERVED_RADIO,  "LoRa SCK"    },
  { 53, PIN_RESERVED_RADIO,  "LoRa NSS"    },
};
const int MEGA_RESERVATION_COUNT = sizeof(MEGA_RESERVATIONS) / sizeof(MEGA_RESERVATIONS[0]);

// Known relay pin assignments on the current Mega node (up to 8 channels)
const uint8_t MEGA_RELAY_PINS[] = { 22, 23, 24, 25, 26, 27, 28, 29 };
const int MEGA_RELAY_COUNT = 8;

// DHT11 pin on the current Mega node (legacy default)
const uint8_t MEGA_DHT_PIN = 7;

// I2C pins on the Mega — advisory reservation when no I2C sensor is configured
const uint8_t MEGA_I2C_SDA = 20;
const uint8_t MEGA_I2C_SCL = 21;

// =====================================================================
//  Expansion module placeholder — for future GPIO expanders
// =====================================================================
enum ExpansionType : uint8_t {
  EXP_NONE    = 0,
  EXP_MCP23017 = 1,  // I2C 16-GPIO expander
  EXP_PCF8574  = 2,  // I2C 8-GPIO expander
};

struct ExpansionModule {
  ExpansionType type;
  const char *name;
  uint8_t    i2cAddress;
  uint8_t    pinCount;
  bool       supported;
  const char *devNote;
};

const ExpansionModule EXPANSION_MODULES[] = {
  { EXP_MCP23017, "MCP23017", 0x20, 16, false, "Add Adafruit MCP23017 library" },
  { EXP_PCF8574,  "PCF8574",  0x27, 8,  false, "Add PCF8574 library" },
};
const int EXPANSION_MODULE_COUNT = sizeof(EXPANSION_MODULES) / sizeof(EXPANSION_MODULES[0]);

// =====================================================================
//  Sensor driver registry — what the firmware actually supports
// =====================================================================
enum SensorType : uint8_t {
  STYPE_NONE       = 0,
  STYPE_DHT11      = 1,
  STYPE_DHT22      = 2,
  STYPE_BME280     = 3,
  STYPE_DS18B20    = 4,
  STYPE_ANALOG     = 5,
  STYPE_VOLTAGE    = 6,
  STYPE_CURRENT    = 7,
  STYPE_WATER_LVL  = 8,
  STYPE_PRESSURE   = 9,
  STYPE_SOIL       = 10,
  STYPE_FLOW       = 11,
  STYPE_CONTACT    = 12,
};

enum BusType : uint8_t {
  BUS_NONE   = 0,
  BUS_GPIO   = 1,
  BUS_I2C    = 2,
  BUS_ANALOG = 3,
};

// How many instances of this sensor type one node can support
enum InstanceLimit : uint8_t {
  LIMIT_ONE  = 1,   // only one per node (e.g. DHT occupies the single DHT slot)
  LIMIT_MANY = 16,  // limited by available pins
};

struct SensorDriver {
  SensorType type;
  const char *name;
  const char *description;
  BusType    bus;
  uint8_t    pinCount;
  uint8_t    pinRequirement;  // CAP_ flag the pin must have
  bool       supported;       // driver exists in firmware
  const char *devNote;        // what's needed if not supported
  // Measurement metadata
  const char *quantity;       // e.g. "temperature", "humidity", "analog"
  const char *unit;           // default display unit
  int16_t    rangeMinX10;     // minimum measurement × 10
  int16_t    rangeMaxX10;     // maximum measurement × 10
  InstanceLimit maxPerNode;   // how many of this type one node supports
  // Version and compatibility
  uint8_t    driverVersion;   // driver definition version (bump on breaking changes)
  const char *fwMinVersion;   // minimum node firmware version required
  const char *hwRequirements; // hardware needed (pull-up, voltage divider, etc.)
};

const SensorDriver SENSOR_DRIVERS[] = {
  // --- Supported drivers (firmware ready) ---
  { STYPE_DHT11,   "DHT11",
    "Temperature & Humidity (±2°C, ±5%RH)",
    BUS_GPIO, 1, CAP_DI, true, nullptr,
    "temperature+humidity", "°C", 0, 500, LIMIT_ONE,
    1, "2.0", "4.7k pull-up on data line" },
  { STYPE_DHT22,   "DHT22",
    "Temperature & Humidity (±0.5°C, ±2%RH)",
    BUS_GPIO, 1, CAP_DI, true, nullptr,
    "temperature+humidity", "°C", -400, 800, LIMIT_ONE,
    1, "2.0", "4.7k pull-up on data line" },
  { STYPE_ANALOG,  "Analog sensor",
    "Generic analog input (soil moisture, light, etc.)",
    BUS_ANALOG, 1, CAP_AI, true, nullptr,
    "analog", "%", 0, 1000, LIMIT_ONE,
    1, "2.0", "0-5V signal to analog pin" },
  // --- Planned drivers (need firmware + hardware work) ---
  { STYPE_BME280,  "BME280",
    "Temperature, Humidity & Pressure",
    BUS_I2C, 0, 0, false,
    "Add Adafruit BME280 library; extend telemetry payload",
    "temperature+humidity+pressure", "°C", -400, 850, LIMIT_ONE,
    1, "3.0", "I2C bus (SDA/SCL), 3.3V supply" },
  { STYPE_DS18B20, "DS18B20",
    "Waterproof temperature probe (±0.5°C)",
    BUS_GPIO, 1, CAP_DI, false,
    "Add OneWire + DallasTemperature libraries; extend telemetry for multi-probe",
    "temperature", "°C", -550, 1250, LIMIT_MANY,
    1, "3.0", "4.7k pull-up, parasitic or 3-wire mode" },
  { STYPE_VOLTAGE, "Voltage sensor",
    "AC/DC voltage measurement (via divider or transformer module)",
    BUS_ANALOG, 1, CAP_AI, false,
    "Implement voltage divider scaling in node firmware; add calibration routine",
    "voltage", "V", 0, 5000, LIMIT_MANY,
    1, "3.0", "Resistor divider or ZMPT101B module, 0-5V output" },
  { STYPE_CURRENT, "Current sensor",
    "AC/DC current measurement (ACS712 or CT clamp)",
    BUS_ANALOG, 1, CAP_AI, false,
    "Implement ACS712 or CT-clamp ADC conversion; extend telemetry",
    "current", "A", 0, 300, LIMIT_MANY,
    1, "3.0", "ACS712 module or SCT-013 CT clamp with burden resistor" },
  { STYPE_WATER_LVL, "Water level sensor",
    "Ultrasonic or float-switch water level",
    BUS_GPIO, 1, CAP_DI, false,
    "Implement HC-SR04 trigger/echo timing or float switch digital input",
    "level", "cm", 0, 4000, LIMIT_MANY,
    1, "3.0", "HC-SR04 (5V, trig+echo pins) or float switch (digital)" },
  { STYPE_PRESSURE, "Pressure sensor",
    "Barometric or process pressure (BMP280 / analog transducer)",
    BUS_I2C, 0, 0, false,
    "Add BMP280 library or analog pressure transducer scaling",
    "pressure", "hPa", 3000, 11000, LIMIT_ONE,
    1, "3.0", "I2C bus or 4-20mA transducer with ADC module" },
  { STYPE_SOIL, "Soil moisture sensor",
    "Capacitive soil moisture (analog output)",
    BUS_ANALOG, 1, CAP_AI, false,
    "Map capacitive sensor ADC range to percentage; needs per-sensor calibration",
    "moisture", "%", 0, 1000, LIMIT_MANY,
    1, "3.0", "Capacitive sensor v1.2+ (3.3-5V), analog output" },
  { STYPE_FLOW, "Flow rate sensor",
    "Pulse-counting flow meter (YF-S201 or similar)",
    BUS_GPIO, 1, CAP_INT, false,
    "Implement pulse counter ISR; convert pulses/sec to L/min; extend telemetry",
    "flow", "L/min", 0, 600, LIMIT_MANY,
    1, "3.0", "Hall-effect flow sensor, 5V supply, interrupt-capable pin" },
  { STYPE_CONTACT, "Digital contact",
    "Door/window sensor, limit switch, or dry contact input",
    BUS_GPIO, 1, CAP_DI, false,
    "Implement digital input polling or interrupt; report open/closed state",
    "contact", "", 0, 10, LIMIT_MANY,
    1, "3.0", "NO or NC dry contact, internal or external pull-up" },
};
const int SENSOR_DRIVER_COUNT = sizeof(SENSOR_DRIVERS) / sizeof(SENSOR_DRIVERS[0]);

// =====================================================================
//  Node instance — a specific controller in this installation
// =====================================================================
const int HW_ID_LEN   = 16;
const int HW_NAME_LEN = 32;
const int MAX_NODES    = 4;

struct NodeInstance {
  char    id[HW_ID_LEN];
  char    name[HW_NAME_LEN];
  uint8_t address;         // LoRa address
  char    board[16];       // "mega2560"
  char    fwVersion[8];    // "2.0"
  bool    active;
};

// =====================================================================
//  HardwareManager — node CRUD, pin state computation
// =====================================================================
// Forward-declared sensor info for dynamic pin state.
// The gateway sets these after config loads so getPinState can reflect
// config-managed sensor assignments instead of hardcoded pins.
struct SensorPinInfo {
  uint8_t    pin;
  uint8_t    sensorType;  // SensorType enum
  const char *name;       // sensor instance name
  bool       active;
};

class HardwareManager {
public:
  NodeInstance nodes[MAX_NODES];
  int nodeCount;

  // Config-managed sensor assignments (set by gateway after config loads)
  SensorPinInfo *configSensors;
  int            configSensorCount;

  HardwareManager() : nodeCount(0), configSensors(nullptr), configSensorCount(0) {
    memset(nodes, 0, sizeof(nodes));
    clearRelayLabels();
  }

  void setSensorPins(SensorPinInfo *sensors, int count) {
    configSensors = sensors;
    configSensorCount = count;
  }

  bool begin() {
    loadNodes();
    return true;
  }

  // ----- Node persistence -----
  void loadNodes() {
    nodeCount = 0;
    File f = LittleFS.open("/config/nodes.json", "r");
    if (!f) return;
    JsonDocument doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return; }
    JsonArray arr = doc.is<JsonArray>() ? doc.as<JsonArray>() : doc["items"].as<JsonArray>();
    for (JsonObject obj : arr) {
      if (nodeCount >= MAX_NODES) break;
      NodeInstance &n = nodes[nodeCount];
      strlcpy(n.id, obj["id"] | "", HW_ID_LEN);
      strlcpy(n.name, obj["name"] | "", HW_NAME_LEN);
      n.address = obj["address"] | 0x02;
      strlcpy(n.board, obj["board"] | "mega2560", sizeof(n.board));
      strlcpy(n.fwVersion, obj["fw"] | "2.0", sizeof(n.fwVersion));
      n.active = true;
      nodeCount++;
    }
    f.close();
  }

  bool saveNodes() {
    LittleFS.mkdir("/config");
    File f = LittleFS.open("/config/nodes.json", "w");
    if (!f) return false;
    JsonDocument doc;
    doc["_v"] = 1;
    JsonArray arr = doc["items"].to<JsonArray>();
    for (int i = 0; i < nodeCount; i++) {
      if (!nodes[i].active) continue;
      JsonObject obj = arr.add<JsonObject>();
      obj["id"]      = nodes[i].id;
      obj["name"]    = nodes[i].name;
      obj["address"] = nodes[i].address;
      obj["board"]   = nodes[i].board;
      obj["fw"]      = nodes[i].fwVersion;
    }
    serializeJson(doc, f);
    f.close();
    return true;
  }

  NodeInstance *findNode(const char *id) {
    for (int i = 0; i < nodeCount; i++)
      if (nodes[i].active && strcmp(nodes[i].id, id) == 0) return &nodes[i];
    return nullptr;
  }

  bool addNode(const char *id, const char *name, uint8_t address,
               const char *board, const char *fw) {
    if (nodeCount >= MAX_NODES) return false;
    if (findNode(id)) return false;
    NodeInstance &n = nodes[nodeCount];
    memset(&n, 0, sizeof(n));
    strlcpy(n.id, id, HW_ID_LEN);
    strlcpy(n.name, name, HW_NAME_LEN);
    n.address = address;
    strlcpy(n.board, board, sizeof(n.board));
    strlcpy(n.fwVersion, fw, sizeof(n.fwVersion));
    n.active = true;
    nodeCount++;
    return saveNodes();
  }

  bool removeNode(const char *id) {
    for (int i = 0; i < nodeCount; i++) {
      if (nodes[i].active && strcmp(nodes[i].id, id) == 0) {
        for (int j = i; j < nodeCount - 1; j++) nodes[j] = nodes[j + 1];
        nodeCount--;
        return saveNodes();
      }
    }
    return false;
  }

  // ----- Auto-create default node if none exist -----
  void ensureDefaultNode() {
    if (nodeCount > 0) return;
    addNode("mega-001", "Main Controller", 0x02, "mega2560", "2.0");
  }

  // ----- Relay-to-equipment name lookup (set by gateway after config loads) -----
  const char *relayUsedBy[MEGA_RELAY_COUNT];

  void clearRelayLabels() {
    for (int i = 0; i < MEGA_RELAY_COUNT; i++)
      relayUsedBy[i] = nullptr;
  }

  // Call after config loads to show equipment names on relay pins
  void setRelayLabel(uint8_t relayIndex, const char *equipmentName) {
    if (relayIndex < MEGA_RELAY_COUNT)
      relayUsedBy[relayIndex] = equipmentName;
  }

  // ----- Pin state query -----
  // Returns the state and "usedBy" string for a given pin on the Mega.
  // Combines static reservations + dynamic assignments (relays, DHT).
  void getPinState(uint8_t pin, PinState &state, const char *&usedBy) {
    // Check system/radio reservations
    for (int i = 0; i < MEGA_RESERVATION_COUNT; i++) {
      PinReservation r;
      memcpy_P(&r, &MEGA_RESERVATIONS[i], sizeof(r));
      if (r.pin == pin) {
        state = r.state;
        usedBy = r.usedBy;
        return;
      }
    }
    // Check relay assignments — show equipment name if assigned
    for (int i = 0; i < MEGA_RELAY_COUNT; i++) {
      if (MEGA_RELAY_PINS[i] == pin) {
        state = PIN_ASSIGNED;
        usedBy = (relayUsedBy[i] && relayUsedBy[i][0]) ? relayUsedBy[i] : "Relay (unassigned)";
        return;
      }
    }
    // Check config-managed sensor assignments
    if (configSensors) {
      for (int i = 0; i < configSensorCount; i++) {
        if (configSensors[i].active && configSensors[i].pin == pin) {
          state = PIN_ASSIGNED;
          usedBy = configSensors[i].name;
          return;
        }
      }
    }
    // I2C pin advisory — reserve SDA/SCL for future I2C sensors
    if (pin == MEGA_I2C_SDA || pin == MEGA_I2C_SCL) {
      state = PIN_RESERVED_SYSTEM;
      usedBy = (pin == MEGA_I2C_SDA) ? "I2C SDA (reserved)" : "I2C SCL (reserved)";
      return;
    }
    state = PIN_AVAILABLE;
    usedBy = "";
  }

  // Check if a pin is available for a new assignment
  bool isPinAvailable(uint8_t pin) {
    PinState st;
    const char *ub;
    getPinState(pin, st, ub);
    return st == PIN_AVAILABLE;
  }

  // Check if a pin has a required capability
  bool pinHasCapability(uint8_t pin, uint8_t requiredCap) {
    for (int i = 0; i < MEGA_PIN_COUNT; i++) {
      PinCap pc;
      memcpy_P(&pc, &MEGA_PINS[i], sizeof(pc));
      if (pc.pin == pin) return (pc.caps & requiredCap) != 0;
    }
    return false;
  }

  // Get pin name string (D0-D53, A0-A15)
  static void pinName(uint8_t pin, char *buf, int bufLen) {
    if (pin >= 54 && pin <= 69) {
      snprintf(buf, bufLen, "A%d", pin - 54);
    } else {
      snprintf(buf, bufLen, "D%d", pin);
    }
  }

  // Build capability tag string
  static void capString(uint8_t caps, char *buf, int bufLen) {
    buf[0] = '\0';
    if (caps & CAP_DI)  strlcat(buf, "DI ",  bufLen);
    if (caps & CAP_DO)  strlcat(buf, "DO ",  bufLen);
    if (caps & CAP_AI)  strlcat(buf, "AI ",  bufLen);
    if (caps & CAP_PWM) strlcat(buf, "PWM ", bufLen);
    if (caps & CAP_INT) strlcat(buf, "INT",  bufLen);
    // Trim trailing space
    int len = strlen(buf);
    if (len > 0 && buf[len - 1] == ' ') buf[len - 1] = '\0';
  }

  // Count available pins
  int countAvailablePins() {
    int count = 0;
    for (int i = 0; i < MEGA_PIN_COUNT; i++) {
      PinCap pc;
      memcpy_P(&pc, &MEGA_PINS[i], sizeof(pc));
      if (isPinAvailable(pc.pin)) count++;
    }
    return count;
  }
};
