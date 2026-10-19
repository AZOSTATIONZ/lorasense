# LoRaSense Commercial Readiness Assessment

## Overview

LoRaSense is an offline-first IoT monitoring and control platform built on
ESP32 (gateway) + Arduino Mega 2560 (node) with SX1278 LoRa radio links at
433 MHz. This document assesses readiness for deployment in real-world
environments.

## Feature Completeness

| Feature | Status | Phase |
|---------|--------|-------|
| Hardware model & config versioning | Done | 1 |
| Sensor catalogue & calibration | Done | 2 |
| Hardware configuration & expansion model | Done | 3 |
| Technician installation wizard | Done | 4 |
| Configuration-driven dashboard | Done | 5 |
| Automation rules with fault handling | Done | 6 |
| Environment profiles & visual themes | Done | 7 |
| Sensor catalogue expansion & driver versioning | Done | 8 |
| HTTP Digest Authentication & role-based access | Done | 9 |
| Persistent event logging | Done | 9 |
| Config backup & restore | Done | 9 |
| System information API | Done | 9 |

## Security Assessment

### Authentication
- **Method**: HTTP Digest Authentication (RFC 7616) via ESP32 WebServer
- **Roles**: Technician (full access) and User (dashboard + control only)
- **Storage**: Credentials stored in `/config/auth.json` on LittleFS
- **Default state**: Auth disabled on first boot (open access for setup)
- **Max users**: 4 credential slots

### Access Control Matrix

| Endpoint Category | User | Technician |
|-------------------|------|------------|
| Dashboard / sensor readings | Yes | Yes |
| Equipment control (on/off) | Yes | Yes |
| Manual override | Yes | Yes |
| View rules & action log | Yes | Yes |
| View events & system info | Yes | Yes |
| Modify environment/zones/equipment | No | Yes |
| Modify rules | No | Yes |
| Add/remove sensors & nodes | No | Yes |
| Onboarding & commissioning | No | Yes |
| Self-test | No | Yes |
| Config backup/restore | No | Yes |
| Auth settings | No | Yes |
| Clear event log | No | Yes |

### Known Limitations
- Passwords stored in plaintext on flash (LittleFS is not encrypted)
- Wi-Fi AP passphrase is the primary perimeter defense
- No session tokens or HTTPS (ESP32 softAP is local-only)
- No brute-force lockout mechanism

### Recommendations
- Change default credentials immediately after first setup
- Use a strong Wi-Fi passphrase (set in secrets.h)
- For sensitive deployments, consider ESP32 modules with flash encryption

## Reliability Assessment

### Fault Handling
- Stale sensor data (>10 min) triggers equipment OFF as fail-safe
- Automation rules have duration and cooldown guards
- Manual override bypasses automation per-output
- 60-second fail-safe timeout for relay commands

### Data Persistence
- All configuration stored on LittleFS flash filesystem
- Config schema versioning for backward-compatible upgrades
- Persistent event log (100 entries, survives reboots)
- Config backup/restore for disaster recovery

### Communication
- LoRa protocol v2 with CRC-16/CCITT-FALSE error detection
- 3-attempt retry with exponential backoff for commands
- Keepalive packets for node connectivity monitoring
- Active-HIGH relay module with fail-safe behavior

## Resource Usage

| Resource | Used | Available | Utilization |
|----------|------|-----------|-------------|
| Flash (gateway) | ~1,053 KB | 1,311 KB | ~80% |
| RAM (gateway) | ~56 KB | 328 KB | ~17% |
| Flash (node) | ~12 KB | 254 KB | ~5% |
| RAM (node) | ~1 KB | 8 KB | ~12% |
| LittleFS partition | Variable | 1,442 KB | ~10-20% |

**Note**: Gateway flash is at 80% — additional large features may require
partition table adjustment or code optimization.

## Deployment Checklist

### Pre-Installation
- [ ] Configure Wi-Fi credentials in `secrets.h`
- [ ] Set auth credentials (or plan to set via wizard)
- [ ] Verify hardware connections match pin assignments
- [ ] Flash both gateway and node firmware
- [ ] Upload LittleFS data partition

### First Boot
- [ ] Connect to LoRaSense Wi-Fi AP
- [ ] Navigate to 192.168.4.1
- [ ] Complete onboarding wizard
- [ ] Verify sensor readings appear on dashboard
- [ ] Test relay control for each equipment output
- [ ] Enable authentication and set passwords
- [ ] Run self-test from Hardware Studio

### Post-Installation
- [ ] Download config backup
- [ ] Verify automation rules trigger correctly
- [ ] Check event log for any fault events
- [ ] Confirm commissioning status is "commissioned"
- [ ] Document physical installation layout

## Architecture Summary

```
[Arduino Mega 2560]              [ESP32 Gateway]
  DHT11 sensor ----+              +---- SX1278 LoRa (433 MHz)
  Analog sensor ---+-- SX1278 -->-+---- Wi-Fi AP (192.168.4.1)
  4x Relay outputs +              +---- LittleFS (web + config)
                                  +---- WebServer (dashboard)
```

- **Single-node architecture** (multi-node deferred)
- **Offline-first**: No internet dependency
- **13 sensor types** in driver catalogue (3 currently supported)
- **3 visual themes**: Friendly, Industrial, Technical

## Version History

| Version | Phase | Description |
|---------|-------|-------------|
| 2.1.0 | 1 | Hardware model & config versioning |
| 2.2.0 | 2 | Sensor catalogue & calibration |
| 2.3.0 | 3 | Hardware configuration model |
| 2.4.0 | 4 | Technician installation wizard |
| 2.5.0 | 5 | Configuration-driven dashboard |
| 2.6.0 | 6 | Automation rules & fault handling |
| 2.7.0 | 7 | Environment profiles & themes |
| 2.8.0 | 8 | Sensor catalogue expansion |
| 2.9.0 | 9 | Commercial readiness (auth, logging, backup) |
