# Phase 0 — Repository and Hardware Audit

**Date:** 2026-10-19
**Audited by:** Claude (senior embedded-systems architect)
**Repository:** `C:\Users\tafad\OneDrive\Desktop\Project2026\lora`

---

## A. Current Architecture

### Data flow (fully implemented)

```
[DHT11/22 sensor] ──D7──► [Arduino Mega 2560] ──SPI──► [SX1278 LoRa]
[Analog sensor]  ──A0-A15─►        │                       │
[4x Relay module] ◄─D22-D25─       │               433 MHz, SF9, 125kHz
                                    │                       │
                              ◄─────┼───────── Protocol v2 ─┤
                                    │                       ▼
                              [SX1278 LoRa] ──SPI──► [ESP32 DevKit]
                                                        │
                                                  Wi-Fi softAP
                                                  192.168.4.1:80
                                                        │
                                                  LittleFS flash
                                                  HTML/CSS/JS + JSON config
                                                        │
                                                   ◄── HTTP ──►
                                                        │
                                                  [Phone/Laptop browser]
```

### Components that exist and work

| Layer | Status |
|-------|--------|
| LoRa radio link (433 MHz point-to-point) | Working |
| Protocol v2 with CRC-16 integrity | Working |
| DHT11/DHT22 temperature + humidity | Working |
| Generic analog input (ADC 0-1023) | Working |
| 4-channel relay control (active HIGH) | Working |
| Non-blocking command handler (3 retries, 1.5s timeout) | Working |
| Config persistence (JSON on LittleFS) | Working |
| Config sync over LoRa (push + ACK) | Working |
| Automation engine with hysteresis (5s eval) | Working |
| Wi-Fi AP + web server | Working |
| Dashboard with sensor hero, charts, device controls | Working |
| Onboarding wizard (5-step first-boot) | Working |
| Hardware Studio (pin map, relays, drivers) | Working |
| Settings CRUD (environment, zones, equipment, sensors, rules) | Working |
| On-device self-test (`/api/test`) | Working |
| Sensor history ring buffer (288 readings, 24h) | Working |
| Canvas-based dual-axis chart (no external libs) | Working |
| Python unit test suite (83 tests) | Working |
| AVR watchdog (8s) on Mega | Working |
| Fail-safe (60s, all relays OFF) on Mega | Working |

### Components that do NOT exist

| Component | Status |
|-----------|--------|
| Database (SQL/NoSQL) | None — all config is flat JSON on LittleFS |
| Backend server (Node, Python, etc.) | None — ESP32 is the server |
| User authentication / sessions | None |
| Multi-node support (runtime) | Scaffolded (MAX_NODES=4, address field) but only one node works |
| OTA firmware update | None |
| Measurement quality / validity flags | None — stale readings not distinguished from fresh |
| Audit / event logging | None — Serial.print only |
| Configuration rollback | None |
| I2C sensor support | Driver registry entries exist (BME280, DS18B20) but firmware drivers not implemented |
| Expansion modules (GPIO expanders) | None |
| Remote / cloud connectivity | None (by design — offline-first) |

---

## B. Verified Hardware

### Confirmed hardware (verified in code and documentation)

| Component | Model | Verified |
|-----------|-------|----------|
| Gateway MCU | ESP32 DevKit (generic) | Yes — firmware compiles for `esp32:esp32:esp32` |
| Node MCU | Arduino Mega 2560 | Yes — firmware compiles for `arduino:avr:mega` |
| LoRa radio × 2 | SX1278 / Ra-02, 433 MHz | Yes — `docs/wiring/pin-map.md`, version register 0x12 |
| Temperature/humidity sensor | DHT11 (using DHT22 library parse mode) | Yes — live readings on serial |
| Relay module | 4-channel, active-HIGH | Yes — pins D22-D25, tested |
| Analog input | Generic (any analog sensor on A0-A15) | Yes — ADC reads in telemetry |

### Documented but not yet connected

| Component | Notes |
|-----------|-------|
| DHT22 (AM2302) | Firmware driver ready, config selectable, not tested with real hardware |
| BME280 | Driver registry entry, marked `supported=false` |
| DS18B20 | Driver registry entry, marked `supported=false` |

### Power note from docs

The Mega's SX1278 is powered from the onboard 3.3V pin at 10 dBm TX. The pin-map document states: "move to a dedicated AMS1117-3.3 regulator before relays are added." Relays have been added. **This power concern may still be open.**

---

## C. Pin Allocation

### ESP32 Gateway

| GPIO | Assignment | Peripheral | Direction | State |
|------|-----------|------------|-----------|-------|
| 5 | LoRa NSS | SX1278 SPI CS | Output | Reserved-radio |
| 14 | LoRa RST | SX1278 reset | Output | Reserved-radio |
| 18 | LoRa SCK | SX1278 SPI clock | Output | Reserved-radio |
| 19 | LoRa MISO | SX1278 SPI data in | Input | Reserved-radio |
| 23 | LoRa MOSI | SX1278 SPI data out | Output | Reserved-radio |
| 26 | LoRa DIO0 | SX1278 interrupt | Input | Reserved-radio |

All other ESP32 GPIO pins are unassigned. The ESP32 runs only Wi-Fi AP + web server + LoRa; it has no directly connected sensors or relays.

### Arduino Mega 2560 Node

| Pin | Assignment | Peripheral | Direction | State | Evidence |
|-----|-----------|------------|-----------|-------|----------|
| D0 | UART0 RX | USB/Serial | Input | Reserved-system | hardware.h |
| D1 | UART0 TX | USB/Serial | Output | Reserved-system | hardware.h |
| D2 | LoRa DIO0 | SX1278 interrupt | Input | Reserved-radio | hardware.h, node-mega.ino |
| D7 | DHT sensor | DHT11/DHT22 data | I/O | Assigned (configurable) | hardware.h, node-mega.ino |
| D9 | LoRa RST | SX1278 reset | Output | Reserved-radio | hardware.h, node-mega.ino |
| D13 | Status LED | Built-in LED | Output | Assigned | node-mega.ino |
| D22 | Relay 1 | Relay module CH1 | Output | Assigned | hardware.h, node-mega.ino |
| D23 | Relay 2 | Relay module CH2 | Output | Assigned | hardware.h, node-mega.ino |
| D24 | Relay 3 | Relay module CH3 | Output | Assigned | hardware.h, node-mega.ino |
| D25 | Relay 4 | Relay module CH4 | Output | Assigned | hardware.h, node-mega.ino |
| D50 | LoRa MISO | SX1278 SPI data in | Input | Reserved-radio | hardware.h, pin-map.md |
| D51 | LoRa MOSI | SX1278 SPI data out | Output | Reserved-radio | hardware.h, pin-map.md |
| D52 | LoRa SCK | SX1278 SPI clock | Output | Reserved-radio | hardware.h, pin-map.md |
| D53 | LoRa NSS | SX1278 SPI CS | Output | Reserved-radio | hardware.h, pin-map.md |
| A0-A15 (D54-D69) | Available | Analog input capable | — | Available | hardware.h |

**Pins NOT in the reservation table but with special functions:**
- D13 is used as a status LED in `node-mega.ino` but NOT marked reserved in `hardware.h`. This means the Hardware Studio would show it as "Available" — a minor inconsistency.
- D14-D21 are UART/I2C pins. Not reserved in code, but the hardware.h pin table comments note their alternate functions. A sensor assigned to D20/D21 would conflict with I2C if BME280 support is added later.

### Conflicts and gaps

1. **D13 not marked as assigned** — cosmetic; won't cause hardware damage but could mislead a technician.
2. **No reservation for potential I2C pins (D20/D21)** — not a problem today but should be flagged when BME280 is enabled.
3. **Analog pin assignment is dynamic** — `configAnalogPin` on the node is set via config sync, not hardcoded. The hardware model (`getPinState`) does not track dynamically assigned analog sensors beyond the config manager's `isSensorPinTaken()` check.

---

## D. Existing Software Capabilities

### Firmware (1,412 lines gateway + 371 lines node)

| Capability | Status | Notes |
|-----------|--------|-------|
| Protocol v2 framing | Complete | Shared header, CRC-16/CCITT-FALSE |
| 9 message types | Complete | CMD_SET, ACK, TELEMETRY, HEARTBEAT, KEEPALIVE, CONFIG_PUSH, CONFIG_ACK, PING, PONG |
| Telemetry (backward compatible) | Complete | 5-byte (old) and 8-byte (new with analog) |
| Non-blocking command queue | Complete | CMD_IDLE → PENDING → SENT → CONFIRMED/FAILED |
| Retry with timeout | Complete | 3 attempts, 1.5s timeout |
| Config sync over LoRa | Complete | CFG_FULL_SYNC push, ACK, 10s retry |
| Dynamic DHT reinitialization | Complete | Pointer-based, pin change at runtime via config sync |
| Fail-safe | Complete | 60s timeout, all relays OFF, configurable via sync |
| Radio watchdog | Complete | 15s deaf → hard reset SX1278 on Mega |
| AVR watchdog | Complete | 8s, wdt_reset() in loop |

### Web dashboard (1,147 lines JS + 348 lines HTML + ~550 lines CSS)

| Feature | Status |
|---------|--------|
| Sensor hero cards (config-driven) | Complete |
| Device control toggles (zone-grouped) | Complete |
| Sensor history chart (canvas, 1h/6h/24h) | Complete |
| Active rule indicators | Complete |
| Connection status banner | Complete |
| Onboarding wizard (5 steps) | Complete |
| Settings: environment, zones, equipment CRUD | Complete |
| Automation rules: add/edit/remove/toggle | Complete |
| Hardware Studio: nodes, pin map, relays, drivers, add-sensor | Complete |

### REST API (26 endpoints)

| Route | Method | Function |
|-------|--------|----------|
| `/api/state` | GET | Dashboard polling (sensors, devices, rules, zones) |
| `/api/devices` | POST | Control a device by equipment ID |
| `/api/config` | GET | Full config dump |
| `/api/config/environment` | POST | Update environment |
| `/api/config/zones` | POST/DELETE | Zone CRUD |
| `/api/config/equipment` | POST/PUT/DELETE | Equipment CRUD |
| `/api/config/sensors` | GET/POST/DELETE | Sensor CRUD |
| `/api/onboarding` | POST | Batch first-time setup |
| `/api/rules` | GET/POST/PUT/DELETE | Automation rule CRUD |
| `/api/history` | GET | Sensor history array |
| `/api/hw/nodes` | GET/POST/DELETE | Node CRUD |
| `/api/hw/pins` | GET | Pin map table |
| `/api/hw/relays` | GET | Relay status table |
| `/api/hw/drivers` | GET | Sensor driver registry |
| `/api/test` | GET | On-device self-test |

### Config persistence (JSON files on LittleFS)

| File | Contents |
|------|----------|
| `/config/env.json` | Environment name and type |
| `/config/zones.json` | Zone list |
| `/config/equipment.json` | Equipment records |
| `/config/sensors.json` | Sensor instances |
| `/config/rules.json` | Automation rules |
| `/config/nodes.json` | Controller node records |

### Tests

| Test | Type | Count |
|------|------|-------|
| `tests/test_protocol.py` | Python unit tests | 83 pass, 0 fail |
| `/api/test` | On-device self-test | 9 checks (CRC, frame, history, LittleFS, config) |
| `firmware/tests/lora-check-esp32/` | Manual radio check | 86 lines |
| `firmware/tests/pingpong-esp32/` | LoRa ping-pong (ESP32 side) | 103 lines |
| `firmware/tests/pingpong-mega/` | LoRa ping-pong (Mega side) | 114 lines |
| `firmware/tests/relay-test-mega/` | Manual relay test | 49 lines |

---

## E. Architectural Weaknesses

### E1. Single-node architecture

The system is architecturally a point-to-point link. `ADDR_NODE1 = 0x02` is hardcoded in the gateway's send routines. The `HardwareManager` supports up to 4 nodes, but:
- All telemetry globals (`nodeTemperature`, `nodeHumidity`, `nodeAnalogValue`) are single-valued — only one node's readings are stored.
- `outputs[]` is a flat 4-element array tied to relay indices 0-3.
- Config sync always pushes to `ADDR_NODE1`.
- The dashboard has no concept of selecting or displaying data from multiple nodes.

**Impact:** Cannot scale to multiple controllers without significant refactoring of the telemetry storage, command routing, config sync, and dashboard.

### E2. No measurement quality model

Sensor readings are global floats (`nodeTemperature`, `nodeHumidity`) with no timestamp, no validity flag, and no staleness check. The automation engine evaluates rules against whatever the last reading was, even if it is hours old. The dashboard shows `node_online` but doesn't indicate when the reading was actually taken.

**Impact:** A stale reading (e.g., after the node goes offline) can look like a current value. The automation engine can trigger rules based on stale data.

### E3. Flat config limits

All config arrays have fixed compile-time limits: `MAX_EQUIPMENT=8`, `MAX_SENSORS=8`, `MAX_ZONES=8`, `MAX_RULES=8`, `MAX_NODES=4`, `MAX_OUTPUTS=4`. These are appropriate for the current hardware but don't scale.

**Impact:** Acceptable for MVP. Will need dynamic allocation or paged storage for larger installations.

### E4. No authentication

The Wi-Fi AP uses WPA2 (passphrase in `secrets.h`), but the web server has no HTTP-level authentication. Anyone connected to the AP can reconfigure the system, control relays, and modify automation rules.

**Impact:** Acceptable for single-user home use. Not acceptable for multi-user, industrial, or commercial deployment.

### E5. No event logging or audit trail

All diagnostic output is `Serial.print`. There is no persistent log of commands issued, rules triggered, config changes, or faults. The sensor history ring buffer stores temperature and humidity only — no event records.

**Impact:** Cannot diagnose issues after the fact. Cannot verify that safety-critical actions (relay switching, fail-safe activation) occurred as expected.

### E6. Monolithic gateway firmware

The gateway `.ino` file is 1,412 lines. Config management, hardware model, web server, LoRa protocol, automation engine, and history buffer are separated into headers, but the web API handlers are all in the main file. This is manageable now but will become unwieldy as features are added.

### E7. Two copies of protocol.h

`firmware/common/protocol.h` and `firmware/node-mega/protocol.h` are currently identical (verified by diff). They must be kept in sync manually. The node includes its local copy; the gateway includes the common copy via relative path.

**Impact:** Any protocol change requires updating both files. A divergence would break communication silently.

### E8. History buffer lacks analog data

`SensorHistory` stores only `tempX10` and `humX10`. Analog sensor readings are not recorded in the history buffer and cannot be charted over time.

### E9. No configuration versioning or migration

Config files are plain JSON with no schema version. If a future firmware adds fields to the config schema, there is no mechanism to migrate existing saved configs.

### E10. Legacy dashboard.h still present

`firmware/gateway-esp32/dashboard.h` contains the original PROGMEM HTML dashboard from before the LittleFS migration. It is not `#include`d anywhere in the current code and is dead code.

### E11. No HTTPS / TLS

The web server is plain HTTP. Traffic between the browser and ESP32 (including config changes and device commands) is unencrypted. Combined with no authentication, this means any wireless sniffer on the AP can observe and replay commands.

---

## F. Proposed Target Architecture

The existing codebase is well-structured and functional. I recommend **extending it incrementally** rather than replacing it. The architecture below maps to the layers in the product spec.

### Layer mapping to existing code

| Target Layer | Current Implementation | Extension Needed |
|-------------|----------------------|------------------|
| A: Hardware capability registry | `hardware.h` — pin caps, reservations, driver registry | Add per-node capability reporting, expansion module support |
| B: Sensor catalogue | `SENSOR_DRIVERS[]` in `hardware.h` | Add versioned sensor definitions with measurement metadata, calibration, validation |
| C: Sensor instances | `SensorInstance` in `config.h` | Add measurement quality, last-read timestamp, calibration params, multiple readings per type |
| D: Equipment registry | `Equipment` in `config.h` | Add equipment type definitions, feedback inputs, interlock model |
| E: Automation engine | `evaluateRules()` in gateway `.ino` | Add duration conditions, cooldown, fault handling, action logging |
| F: Installation config | `ConfigManager` in `config.h` | Add schema versioning, commissioning status, migration |
| G: Measurement history | `SensorHistory` in `history.h` | Add analog history, event log, measurement quality, per-sensor history |
| H: Presentation | `data/` (HTML/CSS/JS) | Environment profiles, visual themes, config-driven layout |

### Key architectural decisions

1. **Stay on ESP32 + LittleFS.** No external database or server. The ESP32 is the entire backend. This matches the offline-first, softAP deployment model.

2. **Keep the protocol v2 framing.** Extend telemetry and config payloads within the existing frame structure (max 64 bytes). Add new message types only when necessary.

3. **Introduce per-node state structs.** Replace the flat globals (`nodeTemperature`, etc.) with a `NodeState` struct per registered node, indexed by address. This is the minimum change needed for multi-node support.

4. **Add config schema versioning.** Every JSON config file gets a `"_v"` field. On load, if the version is older than expected, apply incremental migrations.

5. **Add measurement timestamps and quality.** Each sensor reading gets an `uptimeSec` when it was received and a `quality` flag (fresh / stale / invalid / missing). The automation engine and dashboard use these.

6. **Keep the web stack vanilla.** No frameworks, no build tools, no npm. The dashboard must work on a phone connected to the ESP32's AP with no internet. Canvas charts, no charting libraries. This is a hard constraint from the softAP deployment model.

---

## G. Phased Implementation Plan

### Phase 1 — Document and Stabilise the Existing Hardware

**Objective:** Establish the verified hardware foundation with proper documentation, close known gaps, and ensure existing functionality is preserved.

**Tasks:**
1. Fix D13 pin reservation (add to `MEGA_RESERVATIONS`)
2. Remove dead `dashboard.h` file
3. Add schema version `"_v": 1` to all config JSON files (backward-compatible load)
4. Add measurement timestamp tracking to gateway (uptime when telemetry received)
5. Verify and document the Mega 3.3V power concern from pin-map.md
6. Add integration test: compile both targets, run Python tests, verify no regressions

**Files changed:** `hardware.h`, `config.h`, `gateway-esp32.ino`, `dashboard.h` (deleted)
**Dependencies:** None
**Deliverables:** Updated pin table, config versioning, measurement timestamps
**Tests:** Python test suite (83 existing + new config version tests), both firmware compilations
**Acceptance criteria:** All existing functionality preserved. Hardware facts documented. Pin assignments accurate.
**Hardware required:** Compilation only; existing hardware setup for serial verification
**Risks:** Low. No protocol or behavioral changes.

### Phase 2 — Sensor Catalogue and Multiple Sensor Instances

**Objective:** Create versioned sensor definitions with measurement metadata. Support multiple instances of one type with independent config, calibration, and readings.

**Tasks:**
1. Extend `SensorDriver` with measurement quantity, units, range, calibration schema
2. Add per-instance calibration (offset, scale) to `SensorInstance`
3. Add measurement validity model (fresh / stale / invalid / missing)
4. Validate pin capability vs. sensor requirements on add
5. Prevent conflicting pin assignments across sensors
6. Track last-reading timestamp per sensor instance

**Files changed:** `hardware.h`, `config.h`, `gateway-esp32.ino`, `app.js`
**Dependencies:** Phase 1 (config versioning)
**Deliverables:** Extended sensor model, validity tracking
**Tests:** Python tests for validation logic, firmware compilation, dashboard verification
**Acceptance criteria:** Multiple instances work. Invalid configs rejected. Unsupported types blocked.
**Hardware required:** DHT11 + analog sensor for live test
**Risks:** Medium. Changes to sensor data model affect dashboard rendering.

### Phase 3 — Hardware Configuration and Expansion

**Objective:** Make the Hardware Studio accurate and useful for technicians. Show true pin state, validate assignments, surface conflicts.

**Tasks:**
1. Dynamic pin state that reflects config-manager sensor assignments (not just hardcoded DHT pin)
2. Mark I2C pins (D20/D21) as reserved-for-future when no I2C sensor is configured
3. Show pin capability requirements for each sensor type in the add-sensor form
4. Add expansion module placeholder in the hardware model (for GPIO expanders, future)
5. Improve conflict detection: two sensors on the same pin, relay on a sensor pin

**Files changed:** `hardware.h`, `gateway-esp32.ino`, `app.js`, `index.html`
**Dependencies:** Phase 2
**Deliverables:** Accurate pin map, conflict detection, expansion placeholder
**Tests:** Pin validation tests, UI verification
**Acceptance criteria:** Pin assignments visible and correct. Conflicts explained. Technician-friendly.
**Hardware required:** Compilation + serial for basic verification
**Risks:** Low. UI changes only; no protocol changes.

### Phase 4 — Technician Installation Wizard

**Objective:** Guided commissioning workflow that replaces the current basic onboarding wizard.

**Tasks:**
1. Extend the wizard to include sensor selection and equipment assignment
2. Add environment profile templates (home, farm, greenhouse, industrial, lab, custom)
3. Templates suggest sensors and equipment but don't auto-create hardware
4. Add commissioning status (draft / configured / tested / commissioned)
5. Generate installation summary (viewable on dashboard)
6. Validate completeness before commissioning

**Files changed:** `index.html`, `app.js`, `style.css`, `config.h`, `gateway-esp32.ino`
**Dependencies:** Phase 3
**Deliverables:** Multi-step commissioning wizard, environment profiles
**Tests:** Wizard flow testing, config validation tests
**Acceptance criteria:** Technician can configure an installation. Incomplete config flagged.
**Hardware required:** Browser testing on phone
**Risks:** Medium. Significant UI work. Must preserve existing quick-setup path.

### Phase 5 — Configuration-Driven Dashboard

**Objective:** Dashboard auto-generates from installed sensors and equipment. Multiple sensors of the same type displayed correctly.

**Tasks:**
1. Render sensor hero cards from sensor instances (not hardcoded temp/humidity)
2. Show per-sensor measurement quality (fresh, stale, no data)
3. Display analog sensors with configurable unit labels
4. Show last-reading timestamps
5. Add empty/loading/error states for each sensor card
6. Equipment cards grouped by zone with real-time status

**Files changed:** `app.js`, `style.css`, `gateway-esp32.ino` (API extensions)
**Dependencies:** Phase 2 (sensor instances with quality model)
**Deliverables:** Config-driven dashboard, quality indicators
**Tests:** UI verification across screen sizes, API response verification
**Acceptance criteria:** Dashboard shows exactly what's configured. No phantom sensors. Stale data marked.
**Hardware required:** Phone + ESP32 for responsive testing
**Risks:** Medium. Largest UI change.

### Phase 6 — Automation Rules Enhancement

**Objective:** Structured automation with fault handling, logging, and safety.

**Tasks:**
1. Add duration condition (sensor must exceed threshold for N seconds)
2. Add cooldown period between rule activations
3. Add fault handling: what happens when sensor goes stale during an active rule
4. Log rule evaluations and actions to a ring buffer
5. Add manual override: temporarily disable automation for one equipment
6. Document safety limitations (not a certified safety controller)

**Files changed:** `config.h`, `gateway-esp32.ino`, `app.js`, `index.html`
**Dependencies:** Phase 5 (measurement quality)
**Deliverables:** Enhanced rule engine, action log, fault handling
**Tests:** Hysteresis + duration + cooldown tests, fault scenario tests
**Acceptance criteria:** Rules validated. Faults handled. Actions logged. Safety documented.
**Hardware required:** DHT11 + relays for live rule testing
**Risks:** Medium. Safety-critical logic requires careful testing.

### Phase 7 — Environment Profiles and Visual Themes

**Objective:** Consistent visual system with environment-specific defaults.

**Tasks:**
1. Define 3 visual themes: Home/Farm (friendly), Industrial (dense), Engineering (technical)
2. Environment profile controls suggested sensors, terminology, dashboard layout
3. Theme controls colors, typography, spacing, component styling
4. Accessible contrast ratios for all themes
5. Alarm visibility preserved across all themes

**Files changed:** `style.css`, `app.js`, `index.html`
**Dependencies:** Phase 5 (config-driven dashboard)
**Deliverables:** 3 themes, profile-based defaults
**Tests:** Visual inspection across themes and screen sizes
**Acceptance criteria:** Themes consistent. Alarms visible. Responsive.
**Hardware required:** Phone + laptop browser
**Risks:** Low. CSS-only for themes; JS for profile switching.

### Phase 8 — New Sensor and Expansion Support

**Objective:** Controlled process for adding new sensor types.

**Tasks:**
1. Implement BME280 driver (I2C, temperature + humidity + pressure)
2. Implement DS18B20 driver (OneWire, waterproof temperature)
3. Define driver development workflow and documentation
4. Add firmware compatibility check (driver version vs. firmware version)
5. Versioned sensor definitions in the driver registry

**Files changed:** `node-mega.ino`, `protocol.h` (both copies), `hardware.h`, `config.h`, gateway
**Dependencies:** Phase 3 (pin validation, I2C reservation)
**Deliverables:** 2 new sensor drivers, expansion process documented
**Tests:** Compilation, Python protocol tests for new payloads, hardware testing with sensors
**Acceptance criteria:** New types configurable. Unsupported types clearly marked. Catalog expandable.
**Hardware required:** BME280 + DS18B20 modules for real testing
**Risks:** High. Protocol changes. New hardware dependencies. Two protocol.h files to sync.

### Phase 9 — Commercial Readiness Assessment

**Objective:** Assess readiness for deployment. Not deployment itself.

**Tasks:**
1. Add basic HTTP authentication (digest auth, no TLS cert needed on softAP)
2. Add technician vs. user role concept
3. Add event log persistence (ring buffer to LittleFS)
4. Add config backup/restore via API
5. Document manufacturing test procedure
6. Produce readiness report

**Files changed:** `gateway-esp32.ino`, new `auth.h`, `app.js`
**Dependencies:** All previous phases
**Deliverables:** Auth, logging, backup, readiness report
**Tests:** Auth tests, log persistence tests, backup/restore tests
**Acceptance criteria:** Readiness report produced. No claims of certification.
**Hardware required:** Full system test
**Risks:** Medium. Auth adds complexity. Must not break existing functionality.

---

## H. Questions Requiring Decision

### H1. Power supply for the Mega's SX1278

The pin-map document notes the LoRa module is powered from the Mega's onboard 3.3V regulator at 10 dBm TX, with a recommendation to move to a dedicated AMS1117-3.3 regulator before relays are added. Relays are now in use.

**Question:** Has the dedicated regulator been installed, or is the system still running from the Mega's 3.3V pin? If the latter, are you experiencing any radio reliability issues?

### H2. Multi-node priority

The spec's target architecture describes multiple controllers. The current system supports only one node at runtime.

**Question:** Do you want multi-node support (Phases 1-3 scaffolding) prioritized, or are you focused on single-node capabilities first? Multi-node adds complexity to every layer.

### H3. Remote connectivity

The system is currently softAP-only (no internet). The spec mentions installations that might need remote monitoring.

**Question:** Should the architecture plan for optional STA mode (connect to existing Wi-Fi for remote access), or remain strictly offline-first for all phases?

### H4. Sensor hardware on hand

Phases 2-8 benefit from testing with real sensors.

**Question:** Which sensors do you currently have available? (DHT11, DHT22, BME280, DS18B20, soil moisture, LDR, etc.) This will influence which sensor types get priority driver support.

---

## Summary Statistics

| Metric | Value |
|--------|-------|
| Total source files | 25 (firmware + web + tests + docs) |
| Gateway firmware | 1,412 lines (`.ino` + 4 headers) |
| Node firmware | 371 lines |
| Protocol header | 145 lines (shared, identical copies) |
| Dashboard JS | 1,147 lines |
| Dashboard HTML | 348 lines |
| Dashboard CSS | ~550 lines |
| Python tests | 316 lines, 83 tests, 8 suites |
| Test firmware | 352 lines (4 test sketches) |
| REST API endpoints | 26 |
| Config files (LittleFS) | 6 JSON files |
| Git commits | 19 |
| Gateway flash usage | 79% (1,041,592 / 1,310,720 bytes) |
| Gateway RAM usage | 16% (52,472 / 327,680 bytes) |
| Node flash usage | 4% (11,560 / 253,952 bytes) |
| Node RAM usage | 11% (955 / 8,192 bytes) |

---

**PHASE 0 COMPLETE. No code has been modified.**

Awaiting your review and approval before proceeding to Phase 1.
