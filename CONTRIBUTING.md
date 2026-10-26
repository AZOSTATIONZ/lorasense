# Contributing to LoRaSense

Thank you for your interest in contributing to LoRaSense. This guide explains how to set up the project, add new sensor types, and submit changes.

## Getting Started

### Hardware You'll Need

At minimum:
- ESP32-WROOM-32 DevKit
- Arduino Mega 2560
- 2× SX1278 LoRa modules (433 MHz)
- The sensor you're adding

### Software Setup

1. Install [Arduino IDE](https://www.arduino.cc/en/software) or Arduino CLI
2. Install the ESP32 board package
3. Install required libraries: `LoRa`, `ArduinoJson` (7.x), `DHT sensor library`
4. Clone the repo and create `secrets.h` from the example:

```bash
git clone https://github.com/YOUR_USERNAME/lorasense.git
cd lorasense
cp firmware/gateway-esp32/secrets.example.h firmware/gateway-esp32/secrets.h
```

### Running Tests

Tests are Python scripts that verify the HTML, CSS, and JavaScript source files:

```bash
python tests/test_protocol.py
python tests/test_phase25.py
```

Run tests with `python` directly, not `pytest`.

## Adding a New Sensor — Step by Step

This is the most common contribution. Here's a complete walkthrough using a **pH sensor** as an example.

### Step 1: Register the Driver

Open `firmware/gateway-esp32/hardware.h` and add an entry to the `SENSOR_DRIVERS[]` array:

```cpp
{
  14,                          // type ID (next available number)
  "pH Sensor",                 // name
  "Analog pH probe (PH-4502C module)", // description
  3,                           // bus: 3 = Analog
  1,                           // pin count
  "Any analog pin (A0–A15)",   // pin requirement
  true,                        // supported (true once driver is built)
  "",                          // dev note (empty for supported drivers)
  "pH",                        // quantity measured
  "pH",                        // unit
  0, 140,                      // range: 0.0 – 14.0 (×10 scaled)
  4,                           // max per node
  "1.0",                       // driver version
  "3.0",                       // minimum firmware version
  "PH-4502C module with BNC connector" // hardware requirements
},
```

This makes the sensor appear in the Hardware Studio catalogue and the in-app documentation.

### Step 2: Extend the Telemetry Payload

Open `firmware/node-mega/protocol.h` (and `firmware/common/protocol.h` — keep them in sync) and add a field:

```cpp
struct __attribute__((packed)) TelemetryPayload {
  int16_t  temperatureX10;
  int16_t  humidityX10;
  uint8_t  relayStates;
  int16_t  analogValue;
  uint8_t  analogPin;
  // New field:
  int16_t  phX10;          // pH × 10 (e.g., 72 = 7.2 pH)
};
```

Both the gateway and node must use the same struct, so update both copies.

### Step 3: Write the Read Logic

Open `firmware/node-mega/node-mega.ino` and add the sensor read in `sendTelemetry()`:

```cpp
void sendTelemetry() {
  readSensor();

  TelemetryPayload t;
  t.temperatureX10 = (int16_t)(lastTempC * 10);
  t.humidityX10    = (int16_t)(lastHumid * 10);
  t.relayStates    = relayStateBits();

  // Existing analog
  if (configAnalogPin != 0xFF) {
    t.analogValue = (int16_t)analogRead(configAnalogPin);
    t.analogPin   = configAnalogPin;
  } else {
    t.analogValue = -1;
    t.analogPin   = 0xFF;
  }

  // New: pH sensor on A1
  int rawPh = analogRead(A1);
  float voltage = rawPh * (5.0 / 1024.0);
  float ph = 3.5 * voltage + 0.00;  // calibration formula for PH-4502C
  t.phX10 = (int16_t)(ph * 10);

  // ... send frame
}
```

### Step 4: Verify

After flashing both boards:

1. Connect to the LoRaSense Wi-Fi
2. Open `192.168.4.1`
3. Go to **Hardware Studio** → the new sensor should appear in the catalogue
4. Go to **Dashboard** → the new reading should appear
5. Go to **Automation** → the new sensor should be available as a rule trigger

The dashboard, automation engine, and history system automatically pick up any new field in the telemetry payload.

## Code Style

- **C++ (firmware)**: Arduino style — `camelCase` for functions/variables, `UPPER_CASE` for constants
- **JavaScript**: ES5 syntax (no arrow functions, no `let`/`const`, no template literals) — the ESP32 serves to all browsers including older ones
- **CSS**: BEM-inspired class names, CSS custom properties for theming
- **No build tools**: Everything ships as source files served directly from LittleFS

## Project Structure

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed documentation of the codebase, protocol, and extension points.

## Pull Request Guidelines

1. **One feature per PR** — don't bundle unrelated changes
2. **Test your changes** — run the relevant `test_phase*.py` files
3. **Flash and verify** — compile, flash both boards, verify on the live dashboard
4. **Don't commit secrets.h** — it's git-ignored for a reason
5. **Update the driver catalogue** — if you add a sensor, add its `SensorDriver` entry
6. **Keep protocol.h in sync** — both copies (`node-mega/` and `common/`) must match

## Questions?

Open an issue or reach out. The codebase is documented in:
- [README.md](README.md) — Project overview and getting started
- [ARCHITECTURE.md](ARCHITECTURE.md) — System internals
- [docs/wiring/pin-map.md](docs/wiring/pin-map.md) — Pin assignments
- [docs/littlefs-upload.md](docs/littlefs-upload.md) — Flashing the filesystem
