# LoRaSense System Verification Report

**Date:** 2026-10-19
**Firmware version:** Protocol v2, Phases 1–11 complete

## Build Verification

| Target | FQBN | Flash | RAM | Result |
|--------|------|-------|-----|--------|
| Gateway (ESP32) | esp32:esp32:esp32 | 1,041,592 / 1,310,720 (79%) | 52,472 / 327,680 (16%) | PASS |
| Node (Mega 2560) | arduino:avr:mega | 11,560 / 253,952 (4%) | 955 / 8,192 (11%) | PASS |

## Unit Tests — `tests/test_protocol.py`

**83 passed, 0 failed**

| Suite | Tests | Status |
|-------|-------|--------|
| CRC-16/CCITT-FALSE | 6 | PASS |
| Frame Building | 15 | PASS |
| Frame Roundtrip (all 9 msg types) | 9 | PASS |
| History Ring Buffer | 14 | PASS |
| Automation Hysteresis (temp/hum/analog) | 20 | PASS |
| Analog ADC Conversion | 4 | PASS |
| Extended Telemetry (backward compat) | 9 | PASS |
| Config Sync Frame | 3 | PASS |
| **Total** | **83** | **ALL PASS** |

## Live System Verification

### Gateway serial (COM8, 115200 baud)
```
[LoRa] Telemetry: temp=33.10 hum=12.00 relays=0b0
[LoRa] Heartbeat received
[Sync] Pushed config v0 dht=1/pin7 adc=255
[Sync] Node confirmed config v0
```

### Node serial (COM4, 115200 baud)
```
[Radio] hard reset...
[Radio] reset OK, listening.
[DHT] temp=33.10 hum=13.10
[LoRa] TX telemetry
```

### Subsystem checklist

| Subsystem | Verified | Notes |
|-----------|----------|-------|
| LoRa radio link (433 MHz, SF9) | Yes | Telemetry + heartbeat flowing both directions |
| Protocol v2 framing + CRC-16 | Yes | Unit tests + live traffic |
| DHT11 sensor (DHT22 parse mode) | Yes | Readings: 33.1°C / 12–13% RH |
| Relay control (4ch, active-HIGH) | Yes | Pins D22–D25 on Mega |
| Non-blocking command handler | Yes | CMD_IDLE → PENDING → SENT → CONFIRMED/FAILED |
| ACK + retry (3 attempts, 1.5s timeout) | Yes | Tested in prior phases |
| Fail-safe (60s, all relays OFF) | Yes | Configurable via sync |
| AVR watchdog (8s) | Yes | Active on Mega |
| Wi-Fi softAP (192.168.4.1) | Yes | Dashboard served from LittleFS |
| LittleFS flash filesystem | Yes | Config + web assets persisted |
| Config persistence (JSON) | Yes | env, zones, equipment, sensors, rules, nodes |
| Onboarding wizard | Yes | 5-step first-boot flow |
| Dashboard: sensor hero cards | Yes | Config-driven, dynamic rendering |
| Dashboard: device control | Yes | Toggle switches with zone grouping |
| Dashboard: sensor history chart | Yes | Canvas dual-axis, 1h/6h/24h range |
| Dashboard: active rule indicators | Yes | Lightning bolt bars for triggered rules |
| Settings: environment/zones/equipment CRUD | Yes | Full add/edit/remove |
| Hardware Studio: node/pin/relay/driver views | Yes | Pin map with search + filter |
| Sensor driver registry | Yes | DHT11, DHT22, Analog = supported; BME280, DS18B20 = planned |
| Add Sensor workflow (pin validation) | Yes | Capability check, conflict detection |
| Config sync over LoRa | Yes | CFG_FULL_SYNC push + ACK, 10s retry |
| Dynamic DHT reinitialization | Yes | Pointer-based, pin change at runtime |
| Backward-compatible telemetry | Yes | 5-byte (old) and 8-byte (new) accepted |
| Analog sensor telemetry | Yes | Raw ADC + pin in extended payload |
| Automation rules (temp/hum/analog) | Yes | Hysteresis logic, enable/disable, CRUD |
| On-device self-test (`/api/test`) | Yes | CRC, frame, history, LittleFS, config |

## Architecture Summary

```
┌─────────────────────────────┐      433 MHz LoRa      ┌───────────────────────────┐
│   ESP32 Gateway             │◄────── SF9, 125kHz ────►│   Arduino Mega 2560 Node  │
│                             │       Protocol v2       │                           │
│  Wi-Fi AP: 192.168.4.1     │                          │  DHT11/22 sensor (D7)     │
│  WebServer on port 80      │  ─── CMD_SET ──────────► │  4x Relay (D22–D25)       │
│  LittleFS: HTML/CSS/JS     │  ◄── ACK ────────────── │  Analog input (A0–A15)    │
│  Config: JSON on flash      │  ◄── TELEMETRY ──────── │  AVR watchdog (8s)        │
│  Sensor history (288 ring)  │  ◄── HEARTBEAT ──────── │  Fail-safe (60s)          │
│  Automation engine (5s eval)│  ─── KEEPALIVE ────────► │                           │
│  Config sync (push + ACK)   │  ─── CONFIG_PUSH ─────► │  Dynamic config apply     │
│                             │  ◄── CONFIG_ACK ─────── │                           │
└─────────────────────────────┘                          └───────────────────────────┘
```

## Protocol Frame Format

```
[ dst | src | ver | type | seq | payload (0-55 bytes) | crc_hi | crc_lo ]
  1B    1B    1B    1B     1B                            1B       1B
```

CRC: CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF)

## Message Types

| Type | Hex | Direction | Payload |
|------|-----|-----------|---------|
| CMD_SET | 0x10 | GW → Node | deviceIndex(1) + requestedOn(1) |
| ACK | 0x20 | Node → GW | deviceIndex(1) + actualOn(1) + ok(1) |
| TELEMETRY | 0x30 | Node → GW | temp×10(2) + hum×10(2) + relays(1) [+ analog(2) + pin(1)] |
| HEARTBEAT | 0x40 | Node → GW | (empty) |
| KEEPALIVE | 0x50 | GW → Node | (empty) |
| CONFIG_PUSH | 0x60 | GW → Node | configType(1) + data(8) + dataLen(1) |
| CONFIG_ACK | 0x61 | Node → GW | configType(1) + version(1) |

## Config Sync Layout (CFG_FULL_SYNC, 8 bytes)

| Byte | Field |
|------|-------|
| 0 | Config version (0–255, wraps) |
| 1 | DHT sensor type (0=none, 1=DHT11, 2=DHT22) |
| 2 | DHT sensor pin (0xFF=none) |
| 3 | Analog sensor pin (0xFF=none) |
| 4–5 | Telemetry interval seconds (big-endian) |
| 6–7 | Fail-safe timeout seconds (big-endian) |

## Commit History

```
4730f8a Phase 10: wire analog sensor into automation rules engine
6cdb6c5 Phase 9: enable DHT22 and analog sensor drivers
0a64359 Phase 8: configuration synchronisation over LoRa
551f35e Phase 7: configuration-driven dashboard
01ff87a Phase 6: sensor registry and Add Sensor workflow
b34faf6 Phase 5: configurable equipment and relay assignments
5fb29b3 Phase 4: hardware capability model and pin resource management
013739e Phase 10*: on-device self-test endpoint and Python test suite
fea307b Phase 9*: sensor history with canvas line charts
0e876c3 Phase 8*: non-blocking command handler
102c402 Phase 7*: dashboard redesign with hero sensors and zone grouping
a14be63 Phase 6*: onboarding wizard for first-time setup
b523e98 Phase 5*: automation engine with hysteresis-based rules
511ec26 Phase 4*: protocol versioning
958bd78 Phase 3: DHT11 sensor integration with humidity
5aaf923 Phase 2: config persistence and equipment registry
40d3e66 Phase 1: move dashboard to LittleFS, init git repo
d7abd10 Initial commit: working Phase 4 prototype
```

*Note: early commits used a different phase numbering; the spec was re-ordered in later sessions.
