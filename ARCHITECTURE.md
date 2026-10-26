# Architecture

This document describes the internal architecture of LoRaSense for developers who want to understand, modify, or extend the system.

## System Overview

LoRaSense is a two-board system connected by LoRa radio:

```
┌──────────────────┐                          ┌──────────────────┐
│  SENSOR NODE     │    433 MHz LoRa link     │  GATEWAY         │
│  Arduino Mega    │ ◄──────────────────────► │  ESP32           │
│                  │                          │                  │
│  Reads sensors   │                          │  Serves web UI   │
│  Controls relays │                          │  Runs automation │
│  Executes cmds   │                          │  Stores config   │
└──────────────────┘                          └──────────────────┘
```

The gateway is the brain — it runs the web server, automation engine, configuration storage, and user interface. The node is the muscle — it reads physical sensors and switches relays on command.

## Communication Protocol (v2)

### Frame Format

Every LoRa message uses the same binary frame:

```
Byte:  0     1     2     3     4     5..N-2    N-1   N
     [dst] [src] [ver] [type] [seq] [payload] [crc] [crc]
```

| Field | Size | Description |
|---|---|---|
| dst | 1 byte | Destination address (0x01=gateway, 0x02=node1, 0xFF=broadcast) |
| src | 1 byte | Source address |
| ver | 1 byte | Protocol version (must be 2) |
| type | 1 byte | Message type enum |
| seq | 1 byte | Sequence number (0–255, wraps) |
| payload | 0–55 bytes | Type-specific data |
| crc | 2 bytes | CRC-16/CCITT-FALSE over header+payload |

### Message Types

| Type | Code | Direction | Purpose |
|---|---|---|---|
| CMD_SET | 0x10 | Gateway → Node | Set relay on/off |
| ACK | 0x20 | Node → Gateway | Confirm command result |
| TELEMETRY | 0x30 | Node → Gateway | Sensor readings (every 10s) |
| HEARTBEAT | 0x40 | Node → Gateway | Alive signal (every 30s) |
| KEEPALIVE | 0x50 | Gateway → Node | Prevent fail-safe trigger |
| CONFIG_PUSH | 0x60 | Gateway → Node | Push new configuration |
| CONFIG_ACK | 0x61 | Node → Gateway | Configuration received |

### Telemetry Payload

```c
struct TelemetryPayload {
  int16_t  temperatureX10;  // 245 = 24.5°C
  int16_t  humidityX10;     // 655 = 65.5%RH
  uint8_t  relayStates;     // bit 0 = relay 0, etc.
  int16_t  analogValue;     // 0–1023 or -1 (no sensor)
  uint8_t  analogPin;       // pin number or 0xFF (none)
};
```

Values are scaled to integers (×10) to avoid floating-point on the wire.

### CRC Validation

Both sides compute CRC-16/CCITT-FALSE over the header + payload bytes. The receiver drops any frame where the computed CRC doesn't match the trailing two bytes. This catches bit errors from radio noise.

```
CRC polynomial: 0x1021
Initial value:  0xFFFF
```

### Config Sync

The gateway can push configuration to the node over LoRa. The `CFG_FULL_SYNC` frame packs 8 bytes:

```
[0]    config version (0–255)
[1]    DHT sensor type (1=DHT11, 2=DHT22, 0=none)
[2]    DHT pin number (0xFF=none)
[3]    analog pin number (0xFF=none)
[4-5]  telemetry interval seconds (big-endian)
[6-7]  fail-safe timeout seconds (big-endian)
```

The node applies the config, re-initializes the DHT sensor if the pin changed, and replies with CONFIG_ACK echoing the version number.

## Gateway Architecture (ESP32)

### File Structure

```
gateway-esp32.ino    ← Main sketch: Wi-Fi AP, web server, LoRa RX/TX, automation loop
├── auth.h           ← HTTP Digest Authentication (RFC 7616)
├── config.h         ← JSON configuration (LittleFS read/write)
├── device.h         ← Device model, pin assignments, board abstraction
├── hardware.h       ← Sensor driver registry, board profiles
├── history.h        ← Telemetry ring buffer (in-memory)
└── secrets.h        ← Wi-Fi SSID, passwords, auth credentials (git-ignored)
```

### Request Flow

```
Phone/laptop connects to ESP32 AP (192.168.4.1)
    │
    ├─ GET /              → Serves index.html from LittleFS
    ├─ GET /css/style.css → Serves stylesheet from LittleFS
    ├─ GET /js/app.js     → Serves JavaScript from LittleFS
    │
    ├─ GET /api/status    → Returns JSON: sensor data, relay states, signal strength
    ├─ GET /api/history   → Returns JSON: telemetry ring buffer
    ├─ GET /api/config    → Returns JSON: current device configuration
    ├─ POST /api/config   → Writes config to LittleFS, pushes to node over LoRa
    ├─ POST /api/relay    → Sends CMD_SET to node, returns ACK result
    ├─ GET /api/hw/drivers→ Returns JSON: sensor driver catalogue from hardware.h
    └─ GET /api/hw/boards → Returns JSON: board profile list
```

Every API endpoint is guarded by HTTP Digest Authentication. The `auth.h` module validates the `Authorization` header against credentials stored in `secrets.h` and assigns a role (ROLE_TECHNICIAN or ROLE_USER). Technicians can change configuration; users can view the dashboard and control relays.

### Automation Engine

The gateway runs a rule evaluation loop on every telemetry update:

```
For each active rule:
  1. Read the rule's sensor field from latest telemetry
  2. Compare against threshold (above/below)
  3. Apply hysteresis band to prevent relay chatter
  4. If triggered and not already active:
     - Send CMD_SET to node
     - Wait for ACK
     - Log the action
  5. If the node hasn't been heard from in failsafeTimeoutMs:
     - Mark connection lost
     - Dashboard shows warning
```

Rules are stored as JSON in LittleFS and survive power cycles.

### Storage (LittleFS)

The ESP32's flash is partitioned:

| Partition | Offset | Size | Content |
|---|---|---|---|
| Firmware | 0x10000 | 1,310,720 bytes | Compiled sketch |
| LittleFS | 0x290000 | 1,441,792 bytes | Web files + JSON config |

LittleFS stores:
- `/index.html`, `/css/style.css`, `/js/app.js`, `/manifest.json` — the web dashboard
- `/config.json` — device configuration, environment profile, sensor assignments
- `/rules.json` — automation rules
- `/zones.json` — zone names and assignments

### Telemetry History

The gateway keeps a ring buffer of recent telemetry readings in RAM (not flash, to avoid wear). The buffer holds the last N readings and serves them via `/api/history` for the dashboard's chart. Data is lost on power cycle — this is by design for a flash-constrained device.

## Node Architecture (Arduino Mega)

### Main Loop

```
loop() {
  wdt_reset();                          // Pet the watchdog

  if (LoRa.parsePacket()) {             // Check for incoming
    validate CRC → handleIncoming()
  }

  if (time for telemetry) {             // Every 10s (configurable)
    readSensor() → sendTelemetry()
  }

  if (time for heartbeat) {             // Every 30s
    sendHeartbeat()
  }

  if (radio silent > 15s) {             // Radio watchdog
    hardResetRadio()                    // SX1278 hardware reset via RST pin
  }

  if (gateway silent > 60s) {           // Fail-safe
    allRelaysOff()                      // Safety shutdown
  }
}
```

### Safety Mechanisms

1. **AVR Watchdog (8s)** — If the main loop hangs, the MCU resets
2. **Radio Watchdog (15s)** — If no LoRa packets received, hard-reset the SX1278 via its RST pin
3. **Fail-safe (60s)** — If the gateway hasn't been heard from, turn all relays off
4. **Active-low support** — Relay polarity is configurable (`RELAY_ACTIVE_LOW`)

### Relay Control

Relays are mapped to consecutive pins starting at pin 22:

```
RELAY_PIN[] = { 22, 23, 24, 25, 26, 27, 28, 29 }
RELAY_COUNT = 4  // Change to match your module (1, 2, 4, or 8)
```

State is tracked in a `relayState[]` array and packed into a single byte (bit per relay) for telemetry.

## Web Dashboard Architecture

### Single-Page Application

The entire dashboard is a single HTML file with one CSS stylesheet and one JavaScript file. No frameworks, no build tools, no npm. This keeps the total size under the LittleFS partition limit and eliminates external dependencies.

```
index.html  — Page shell, navigation, all section markup
style.css   — Complete stylesheet (themes, dark mode, responsive, animations)
app.js      — All application logic (fetch, render, state management)
```

### Page Sections

| Section | Purpose |
|---|---|
| Dashboard | Real-time sensor cards, sparklines, range indicators |
| History | Dual-axis canvas chart with zoom and export |
| Controls | Relay toggle switches with live state |
| Automation | Rule builder with environment-specific presets |
| Settings | Device config, sensor assignments, environment profile |
| Hardware Studio | Pin map, driver catalogue, board profiles |
| Documentation | 8-section help page with wiring guides and FAQ |

### Theming System

Three visual themes are available, selected by environment profile:

| Theme | CSS Class | Used By |
|---|---|---|
| Friendly | `theme-friendly` | Home, Farm, Greenhouse |
| Industrial | `theme-industrial` | Industrial |
| Technical | `theme-technical` | Lab, Custom |

Each theme defines a set of CSS custom properties (colors, borders, accents). Dark mode is layered on top via `[data-theme="dark"]` and `prefers-color-scheme: dark`.

### Chart Rendering

Charts are drawn directly on HTML5 Canvas with no charting library:

1. Fetch `/api/history` for the data array
2. Compute min/max for each axis (temperature left, humidity right)
3. Draw grid lines and axis labels
4. Plot each data series as a connected line
5. Draw sparklines in the sensor hero cards using the same data

This approach keeps the JavaScript small and avoids CDN dependencies.

### Icon System

The dashboard uses 23 inline SVG icons defined in a JavaScript `svgIcon()` function. No icon fonts, no external icon libraries. Each icon is a `<svg>` element returned as an HTML string.

## Sensor Driver Registry

The gateway maintains a driver catalogue in `hardware.h`:

```c
struct SensorDriver {
  uint8_t     type;
  const char* name;
  const char* desc;
  uint8_t     bus;           // 0=GPIO, 1=I2C, 2=SPI, 3=Analog, 4=UART
  uint8_t     pinCount;
  const char* pinRequirement;
  bool        supported;     // true = driver built and tested
  const char* devNote;       // what's needed to implement
  const char* quantity;      // "Temperature", "Humidity", etc.
  const char* unit;          // "°C", "%RH", etc.
  int16_t     rangeMin, rangeMax;  // ×10 scaled
  uint8_t     maxPerNode;
  const char* driverVersion;
  const char* fwMinVersion;
  const char* hwRequirements;
};
```

The catalogue is served via `/api/hw/drivers` and rendered in the Hardware Studio section. Adding a new sensor starts here — register it in the `SENSOR_DRIVERS[]` array.

## Security Model

### Authentication

- HTTP Digest Authentication (RFC 7616) — challenge-response, no plaintext passwords
- Nonce-based to prevent replay attacks
- Two roles: ROLE_TECHNICIAN (full config access) and ROLE_USER (dashboard + relay control)

### Credential Storage

- Credentials stored in `secrets.h` which is git-ignored
- `secrets.example.h` provides a template
- Credentials are compiled into firmware, not stored in LittleFS

### Network Isolation

- The ESP32 runs as a standalone AP — no internet connection
- No data leaves the local network
- No cloud services, no telemetry, no analytics
- The only way to access data is to connect to the AP's Wi-Fi

## Extension Points

### Adding a Sensor

1. Add a `SensorDriver` entry in `hardware.h` → appears in the catalogue
2. Add a field to `TelemetryPayload` in `protocol.h` → carried over LoRa
3. Add the read call in `node-mega.ino` → node reads the hardware
4. The dashboard, automation, and history pick it up automatically

### Adding a New Environment Profile

1. Add a profile object in `app.js` in the `ENVIRONMENTS` array
2. Define: name, theme, ideal ranges, terminology overrides, automation presets, tips
3. The settings page, dashboard, and automation engine adapt automatically

### Adding a New Dashboard Section

1. Add the HTML section in `index.html` with a unique `id`
2. Add a nav item in the navigation bar
3. Add the show/hide logic in `app.js` navigation handler
4. Add styles in `style.css`

## Build and Flash Pipeline

```
Source files
    │
    ├─ arduino-cli compile ──► .bin firmware image
    │                              │
    │                              └─ arduino-cli upload ──► Flash to board via USB
    │
    └─ mklittlefs -c data/ ──► littlefs.bin filesystem image
                                   │
                                   └─ esptool write_flash ──► Flash to ESP32 partition
```

No CI/CD pipeline — builds run locally on the developer's machine. The Arduino CLI handles compilation and upload; mklittlefs packages the web files into a flashable image.
