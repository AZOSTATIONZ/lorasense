# LoRaSense

**An offline-first IoT monitoring platform built on LoRa radio and ESP32.**

LoRaSense is a complete, production-grade sensor monitoring and relay control system that runs entirely offline — no cloud, no internet, no account required. It uses long-range LoRa radio to connect sensor nodes to a Wi-Fi gateway that serves a full-featured dashboard from the ESP32's own flash storage.

Built for farms, greenhouses, industrial facilities, and labs where reliable monitoring matters more than cloud connectivity.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32%20%2B%20Arduino%20Mega-brightgreen.svg)
![Phase](https://img.shields.io/badge/development-Phase%2025%20%2F%2025-success.svg)
![Tests](https://img.shields.io/badge/tests-1%2C300%2B%20passing-success.svg)

---

## Why LoRaSense?

| Problem | LoRaSense approach |
|---|---|
| Cloud IoT platforms need internet and subscriptions | Runs on a local Wi-Fi hotspot, zero external dependencies |
| Sensor-to-server wiring is expensive at range | LoRa radio reaches 1+ km on a $3 module |
| Commercial systems are locked to specific sensors | Open driver registry — add any sensor with ~30 lines of C++ |
| Most DIY projects stop at "it reads a DHT11" | Full dashboard with auth, automation, history, charts, dark mode, PWA |

## Key Features

- **Long-range wireless** — SX1278 LoRa modules at 433 MHz, SF9, 125 kHz bandwidth
- **Self-hosted dashboard** — Responsive web UI served from ESP32 flash (LittleFS), accessible at `192.168.4.1`
- **Role-based authentication** — HTTP Digest Auth (RFC 7616) with Technician and User roles
- **6 environment profiles** — Home, Farm, Greenhouse, Industrial, Lab, Custom — each with tailored terminology, ideal ranges, tips, and automation presets
- **Automation engine** — Threshold-based rules with hysteresis, fault handling, and fail-safe relay shutdown
- **Dynamic relay control** — 1/2/4/8 channel relay module support, configurable per deployment
- **13 sensor types** in the driver catalogue (3 production-ready, 10 on the engineering roadmap)
- **Real-time charts** — Canvas-based dual-axis line charts with sparklines, no external charting library
- **Dark mode** — System-aware with manual toggle, persisted across sessions
- **Offline PWA** — Installable on phones, works without internet
- **CSV export** — Download sensor history for analysis
- **Accessible** — Keyboard navigation, ARIA labels, screen reader support
- **In-app documentation** — 8-section help page with wiring guides, FAQ, and sensor catalogue

## Architecture

```
┌─────────────────────┐         433 MHz LoRa         ┌─────────────────────┐
│   SENSOR NODE       │ ◄──────────────────────────── │   GATEWAY           │
│   Arduino Mega 2560 │                               │   ESP32-WROOM-32    │
│                     │ ──────────────────────────►   │                     │
│  • DHT11/22 sensor  │   Telemetry (10s interval)    │  • Wi-Fi AP         │
│  • Analog inputs    │   Heartbeat (30s)             │  • Web server       │
│  • Relay outputs    │   Relay commands              │  • LittleFS storage │
│  • SX1278 radio     │   Config sync                 │  • SX1278 radio     │
│  • Watchdog timer   │                               │  • REST API         │
└─────────────────────┘                               └─────────────────────┘
                                                              │
                                                              │ Wi-Fi (192.168.4.1)
                                                              ▼
                                                      ┌─────────────────────┐
                                                      │   ANY DEVICE        │
                                                      │   Phone / Laptop    │
                                                      │                     │
                                                      │  • Dashboard        │
                                                      │  • Settings         │
                                                      │  • Hardware Studio  │
                                                      │  • Automation rules │
                                                      └─────────────────────┘
```

### Protocol

Communication uses a custom binary protocol (v2) over LoRa:

```
Frame: [dst | src | ver | type | seq | payload | crc_hi | crc_lo]
```

- **CRC-16/CCITT-FALSE** error detection
- Message types: Telemetry, Heartbeat, Command, ACK, Config Sync
- Compact payloads — a full telemetry frame fits in ~12 bytes
- Sequence numbers for command deduplication

### Data Flow

```
Sensor hardware → Node firmware reads value
    → Scales to integer (24.5°C → 245)
    → Packs into TelemetryPayload struct
    → Wraps in protocol v2 frame with CRC
    → Transmits over LoRa at 433 MHz
    → Gateway receives and validates CRC
    → Unpacks struct, stores in memory
    → Serves via /api/status JSON endpoint
    → Dashboard polls every 2 seconds
    → Renders in hero cards, charts, automation engine
```

## Hardware Requirements

### Bill of Materials

| Component | Quantity | Purpose | Approx. Cost |
|---|---|---|---|
| ESP32-WROOM-32 DevKit | 1 | Gateway (Wi-Fi AP + web server) | $5 |
| Arduino Mega 2560 | 1 | Sensor node (I/O + relay control) | $12 |
| SX1278 LoRa module (433 MHz) | 2 | Wireless link (one per board) | $3 each |
| DHT22 sensor | 1 | Temperature + humidity | $3 |
| 4-channel relay module | 1 | Output control | $4 |
| 4.7kΩ resistor | 1 | Pull-up for DHT data line | $0.01 |
| Jumper wires | ~20 | Connections | $2 |
| **Total** | | | **~$33** |

### Wiring Summary

**Gateway (ESP32 → SX1278 SPI):**

| ESP32 | SX1278 |
|---|---|
| GPIO 5 | NSS |
| GPIO 18 | SCK |
| GPIO 23 | MOSI |
| GPIO 19 | MISO |
| GPIO 26 | RST |
| GPIO 33 | DIO0 |

**Node (Mega → Sensors + Relays):**

| Mega Pin | Connection |
|---|---|
| Pin 7 | DHT22 data (with 4.7kΩ pull-up to 5V) |
| A0 | Analog sensor input |
| Pins 22–29 | Relay module channels 1–8 |
| SPI (50–53) | SX1278 LoRa module |

## Getting Started

### Prerequisites

- [Arduino IDE](https://www.arduino.cc/en/software) or Arduino CLI
- ESP32 board package installed
- Libraries: `LoRa`, `ArduinoJson` (7.x), `DHT sensor library`
- Python 3.8+ (for running tests)

### 1. Clone and configure

```bash
git clone https://github.com/YOUR_USERNAME/lorasense.git
cd lorasense
```

Create your credentials file:

```bash
cp firmware/gateway-esp32/secrets.example.h firmware/gateway-esp32/secrets.h
```

Edit `secrets.h` with your Wi-Fi AP name and authentication credentials.

### 2. Flash the gateway (ESP32)

```bash
# Compile
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/gateway-esp32/gateway-esp32.ino

# Flash firmware
arduino-cli upload --fqbn esp32:esp32:esp32 --port COM8 firmware/gateway-esp32/gateway-esp32.ino

# Build and flash the web dashboard (LittleFS)
mklittlefs -c firmware/gateway-esp32/data -p 256 -b 4096 -s 1441792 littlefs.bin
esptool --chip esp32 --port COM8 write_flash 0x290000 littlefs.bin
```

### 3. Flash the node (Arduino Mega)

```bash
arduino-cli compile --fqbn arduino:avr:mega firmware/node-mega/node-mega.ino
arduino-cli upload --fqbn arduino:avr:mega --port COM4 firmware/node-mega/node-mega.ino
```

### 4. Connect and configure

1. Power on both boards
2. Connect to the **LoRaSense** Wi-Fi network from your phone or laptop
3. Open **192.168.4.1** in your browser
4. Complete the setup wizard — select your environment, name your zones, commission sensors
5. Set up automation rules from the presets or create custom ones

## Project Structure

```
lorasense/
├── firmware/
│   ├── gateway-esp32/          # ESP32 gateway firmware
│   │   ├── gateway-esp32.ino   # Main gateway sketch
│   │   ├── auth.h              # HTTP Digest Authentication (RFC 7616)
│   │   ├── config.h            # JSON configuration (LittleFS)
│   │   ├── device.h            # Device model and pin assignments
│   │   ├── hardware.h          # Sensor driver registry + board profiles
│   │   ├── history.h           # Telemetry history ring buffer
│   │   ├── secrets.example.h   # Template for credentials (copy to secrets.h)
│   │   └── data/               # Web dashboard (served from LittleFS)
│   │       ├── index.html      # Single-page application shell
│   │       ├── css/style.css   # Full stylesheet with 3 themes + dark mode
│   │       ├── js/app.js       # Dashboard, settings, studio, docs — all JS
│   │       └── manifest.json   # PWA manifest
│   ├── node-mega/              # Arduino Mega sensor node
│   │   ├── node-mega.ino       # Main node sketch (sensor reading + relay control)
│   │   └── protocol.h          # Shared protocol v2 definitions
│   ├── common/
│   │   └── protocol.h          # Canonical protocol definition
│   └── tests/                  # Hardware verification sketches
│       ├── lora-check-esp32/   # LoRa module connectivity test
│       ├── pingpong-esp32/     # LoRa TX/RX ping-pong test
│       ├── pingpong-mega/      # LoRa ping-pong (Mega side)
│       └── relay-test-mega/    # Relay module channel test
├── tests/                      # Python test suite (1,300+ assertions)
│   ├── test_protocol.py        # Protocol encoding/CRC tests
│   ├── test_auth.py            # Authentication logic tests
│   ├── test_profiles.py        # Environment profile tests
│   ├── test_sensor_catalogue.py
│   └── test_phase*.py          # Per-phase regression tests (12–25)
├── docs/
│   ├── PHASE0-AUDIT.md         # Initial hardware audit
│   ├── VERIFICATION.md         # Verification procedures
│   ├── READINESS.md            # Production readiness assessment
│   ├── littlefs-upload.md      # LittleFS flash guide
│   └── wiring/
│       └── pin-map.md          # Complete pin assignment map
└── .gitignore
```

## Dashboard Screenshots

> Screenshots will be added after first deployment. The dashboard includes:
> - Real-time sensor hero cards with sparkline charts and range indicators
> - Dual-axis history chart with canvas rendering
> - Environment tips and ideal range badges
> - Hardware Studio with pin map and driver catalogue
> - Automation rule builder with environment-specific presets
> - In-app documentation with collapsible sections
> - Dark mode with system-aware toggle

## Sensor Catalogue

| Sensor | Type | Bus | Status |
|---|---|---|---|
| DHT11 | Temperature + Humidity | GPIO | **Supported** |
| DHT22 / AM2302 | Temperature + Humidity | GPIO | **Supported** |
| Analog Input | Generic 0–100% | Analog | **Supported** |
| BME280 | Temp + Humidity + Pressure | I2C | Engineering |
| DS18B20 | Waterproof Temperature | GPIO | Engineering |
| Voltage Sensor | AC/DC Voltage | Analog | Engineering |
| Current Sensor | AC/DC Current | Analog | Engineering |
| Water Level | Ultrasonic / Float | GPIO | Engineering |
| Pressure Sensor | Barometric / Process | I2C | Engineering |
| Soil Moisture | Capacitive | Analog | Engineering |
| Flow Rate | Pulse-counting | GPIO | Engineering |
| Digital Contact | Door/Window Switch | GPIO | Engineering |

**Supported** = driver built, tested, and ready to commission from the dashboard.
**Engineering** = hardware interface defined, firmware driver not yet built. See [CONTRIBUTING.md](CONTRIBUTING.md) for how to add one.

## Adding a New Sensor

Adding a sensor type requires changes in 3 files (~30–50 lines total):

1. **Register the driver** — Add a `SensorDriver` entry in `firmware/gateway-esp32/hardware.h`
2. **Extend the telemetry packet** — Add a field to `TelemetryPayload` in `firmware/node-mega/protocol.h`
3. **Write the read logic** — Add the sensor read call in `firmware/node-mega/node-mega.ino`

The dashboard, automation engine, and history system pick up new sensor data automatically. See [CONTRIBUTING.md](CONTRIBUTING.md) for a step-by-step walkthrough with a pH sensor example.

## Development

### Running Tests

```bash
# Run all phase tests
python tests/test_phase12.py
python tests/test_phase13.py
# ... through test_phase25.py

# Run protocol tests
python tests/test_protocol.py

# Run auth tests
python tests/test_auth.py
```

Tests verify HTML structure, CSS styles, JavaScript logic, and protocol encoding. All 1,300+ assertions pass.

### Build Stats

| Metric | Value |
|---|---|
| Gateway flash usage | 1,074,576 / 1,310,720 bytes (81%) |
| Gateway RAM usage | 56,224 / 327,680 bytes (17%) |
| LittleFS image | ~50 KB compressed |
| Web dashboard | 1 HTML + 1 CSS + 1 JS (no frameworks) |
| External dependencies | 0 (no CDN, no npm, no build tools) |
| Total commits | 46 across 25 development phases |

## Design Decisions

| Decision | Rationale |
|---|---|
| **No framework (vanilla JS)** | Fits in ESP32 flash, loads instantly, no build tooling |
| **Single-node architecture** | Simplicity first; multi-node deferred until single-node is solid |
| **LoRa at 433 MHz** | License-free ISM band, long range, low power |
| **HTTP Digest Auth** | Secure without HTTPS (no TLS certificate needed on a local AP) |
| **LittleFS over SPIFFS** | Better wear leveling, directory support, actively maintained |
| **ArduinoJson 7.x** | Efficient serialization on constrained devices |
| **Canvas charts** | No library dependency, full control, works offline |
| **Offline-first** | The system works with zero internet — by design, not as a fallback |

## Advantages

- **Zero recurring cost** — No cloud subscriptions, no SIM cards, no API fees
- **Works anywhere** — No internet needed; deploy in remote fields, basements, or off-grid sites
- **Long range** — LoRa reaches 1+ km line-of-sight between node and gateway
- **Low power** — Sensor node can run on battery or solar with a sleep mode extension
- **Open and extensible** — Add sensors, change profiles, build new automation rules
- **Professional UI** — Not a hobbyist serial monitor; a real dashboard suitable for non-technical users
- **Secure** — Digest auth with roles; no plaintext passwords over the wire
- **Portable** — Same hardware kit configures for any environment through software

## Limitations

- **Single node** — Current firmware supports one sensor node per gateway (multi-node is architecturally planned but not implemented)
- **No internet telemetry** — Data stays local; no remote monitoring unless you add an uplink
- **Limited flash** — ESP32 has ~1.3 MB for firmware and ~1.4 MB for the filesystem; the dashboard is already at 81% flash
- **Manual sensor drivers** — Adding a new sensor type requires C++ firmware changes and reflashing
- **No OTA updates** — Firmware updates require physical USB connection
- **433 MHz regulations** — Duty cycle and power limits vary by country; check your local ISM band rules

## Roadmap

- [ ] Multi-node support (up to 8 nodes per gateway)
- [ ] OTA firmware updates via the dashboard
- [ ] MQTT bridge for optional cloud connectivity
- [ ] Battery monitoring and low-power sleep mode
- [ ] Sensor calibration UI
- [ ] Data logging to SD card
- [ ] Alert notifications via buzzer or LED

## Built With

- **ESP32** — Espressif Wi-Fi + Bluetooth SoC
- **Arduino Mega 2560** — ATmega2560 microcontroller
- **SX1278** — Semtech LoRa transceiver (433 MHz)
- **LoRa library** — Arduino LoRa by Sandeep Mistry
- **ArduinoJson 7.4** — JSON serialization for embedded
- **DHT library** — Adafruit DHT sensor library
- **LittleFS** — Flash filesystem for ESP32
- **Vanilla JavaScript** — No frameworks, no build tools

## License

This project is licensed under the MIT License — see [LICENSE](LICENSE) for details.

## Author

**Tafadzwa Musendo**

Built from hardware to dashboard as a solo full-stack embedded systems project, demonstrating end-to-end IoT development: circuit design, wireless protocol engineering, embedded C++ firmware, real-time web dashboard, and deployment-ready configuration management.
