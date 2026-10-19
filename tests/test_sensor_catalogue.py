#!/usr/bin/env python3
"""
Phase 2 tests: sensor catalogue, calibration, measurement quality,
and instance-limit validation.

Mirrors the C++ logic from hardware.h, config.h, and gateway-esp32.ino
so we can test it without flashing.
"""
import sys, math

passed = 0
failed = 0

def check(label, condition):
    global passed, failed
    if condition:
        passed += 1
        print(f"  PASS  {label}")
    else:
        failed += 1
        print(f"  FAIL  {label}")

# =====================================================================
#  Measurement quality model (mirrors measureQuality())
# =====================================================================
print("\n== Measurement quality ==")

MQ_FRESH   = 0
MQ_STALE   = 1
MQ_INVALID = 2
MQ_MISSING = 3

HISTORY_INTERVAL_MS = 300_000  # 5 minutes

def measure_quality(has_reading, age_ms, value, min_x10, max_x10):
    if not has_reading:
        return MQ_MISSING
    if math.isnan(value) or int(value * 10) < min_x10 or int(value * 10) > max_x10:
        return MQ_INVALID
    if age_ms > HISTORY_INTERVAL_MS * 2:
        return MQ_STALE
    return MQ_FRESH

# Fresh reading within range
check("fresh temperature 25.0°C",
      measure_quality(True, 60_000, 25.0, 0, 500) == MQ_FRESH)

check("fresh humidity 50%",
      measure_quality(True, 60_000, 50.0, 0, 1000) == MQ_FRESH)

# Stale (>10 min old)
check("stale after 11 minutes",
      measure_quality(True, 660_000, 25.0, 0, 500) == MQ_STALE)

# Exactly at boundary (10 min = not stale)
check("not stale at exactly 10 min",
      measure_quality(True, 600_000, 25.0, 0, 500) == MQ_FRESH)

# Invalid: out of range
check("invalid below range",
      measure_quality(True, 60_000, -5.0, 0, 500) == MQ_INVALID)

check("invalid above range DHT11",
      measure_quality(True, 60_000, 55.0, 0, 500) == MQ_INVALID)

check("valid at range boundary 0°C",
      measure_quality(True, 60_000, 0.0, 0, 500) == MQ_FRESH)

check("valid at range boundary 50°C",
      measure_quality(True, 60_000, 50.0, 0, 500) == MQ_FRESH)

# Invalid: NaN
check("invalid NaN",
      measure_quality(True, 60_000, float('nan'), 0, 500) == MQ_INVALID)

# Missing: no reading ever
check("missing when no reading",
      measure_quality(False, 0, 0.0, 0, 500) == MQ_MISSING)

# DHT22 wider range
check("DHT22 -40°C valid",
      measure_quality(True, 60_000, -40.0, -400, 800) == MQ_FRESH)

check("DHT22 80°C valid",
      measure_quality(True, 60_000, 80.0, -400, 800) == MQ_FRESH)

check("DHT22 -41°C invalid",
      measure_quality(True, 60_000, -41.0, -400, 800) == MQ_INVALID)


# =====================================================================
#  Calibration math (mirrors applyCalibration())
# =====================================================================
print("\n== Calibration ==")

def apply_calibration(raw, offset_x10, scale_x100):
    return (raw * scale_x100 / 100.0) + (offset_x10 / 10.0)

# No calibration (identity)
check("identity calibration",
      abs(apply_calibration(25.0, 0, 100) - 25.0) < 0.01)

# Offset only: +2.5°C
check("offset +2.5°C",
      abs(apply_calibration(25.0, 25, 100) - 27.5) < 0.01)

# Negative offset: -1.0°C
check("offset -1.0°C",
      abs(apply_calibration(25.0, -10, 100) - 24.0) < 0.01)

# Scale only: 1.05× (5% gain)
check("scale 1.05x",
      abs(apply_calibration(100.0, 0, 105) - 105.0) < 0.01)

# Scale + offset combined
check("scale 0.95x + offset -0.5",
      abs(apply_calibration(20.0, -5, 95) - 18.5) < 0.01)

# Zero input
check("zero raw value",
      abs(apply_calibration(0.0, 10, 100) - 1.0) < 0.01)


# =====================================================================
#  Instance-limit validation model
# =====================================================================
print("\n== Instance limits ==")

LIMIT_ONE  = 1
LIMIT_MANY = 16

# Sensor driver registry (mirrors SENSOR_DRIVERS[] from hardware.h)
STYPE_DHT11     = 1
STYPE_DHT22     = 2
STYPE_BME280    = 3
STYPE_DS18B20   = 4
STYPE_ANALOG    = 5
STYPE_VOLTAGE   = 6
STYPE_CURRENT   = 7
STYPE_WATER_LVL = 8
STYPE_PRESSURE  = 9
STYPE_SOIL      = 10
STYPE_FLOW      = 11
STYPE_CONTACT   = 12

DRIVER_LIMITS = {
    STYPE_DHT11:     LIMIT_ONE,
    STYPE_DHT22:     LIMIT_ONE,
    STYPE_BME280:    LIMIT_ONE,
    STYPE_DS18B20:   LIMIT_MANY,
    STYPE_ANALOG:    LIMIT_ONE,
    STYPE_VOLTAGE:   LIMIT_MANY,
    STYPE_CURRENT:   LIMIT_MANY,
    STYPE_WATER_LVL: LIMIT_MANY,
    STYPE_PRESSURE:  LIMIT_ONE,
    STYPE_SOIL:      LIMIT_MANY,
    STYPE_FLOW:      LIMIT_MANY,
    STYPE_CONTACT:   LIMIT_MANY,
}

DRIVER_SUPPORTED = {
    STYPE_DHT11: True, STYPE_DHT22: True, STYPE_ANALOG: True,
    STYPE_BME280: False, STYPE_DS18B20: False,
    STYPE_VOLTAGE: False, STYPE_CURRENT: False, STYPE_WATER_LVL: False,
    STYPE_PRESSURE: False, STYPE_SOIL: False, STYPE_FLOW: False, STYPE_CONTACT: False,
}

def count_on_node(sensors, stype, node_id):
    return sum(1 for s in sensors if s['type'] == stype and s['node_id'] == node_id and s['active'])

def can_add_sensor(sensors, stype, node_id):
    limit = DRIVER_LIMITS.get(stype, 1)
    return count_on_node(sensors, stype, node_id) < limit

# Empty node — can add anything
check("can add DHT11 to empty node",
      can_add_sensor([], STYPE_DHT11, "mega-001"))

check("can add analog to empty node",
      can_add_sensor([], STYPE_ANALOG, "mega-001"))

# One DHT11 already — cannot add second
existing = [{'type': STYPE_DHT11, 'node_id': 'mega-001', 'active': True}]
check("cannot add second DHT11",
      not can_add_sensor(existing, STYPE_DHT11, "mega-001"))

# But can add a DHT11 to a DIFFERENT node
check("can add DHT11 to different node",
      can_add_sensor(existing, STYPE_DHT11, "mega-002"))

# One DHT11 — can still add analog
check("can add analog alongside DHT11",
      can_add_sensor(existing, STYPE_ANALOG, "mega-001"))

# One analog already — cannot add second analog
existing_analog = [{'type': STYPE_ANALOG, 'node_id': 'mega-001', 'active': True}]
check("cannot add second analog",
      not can_add_sensor(existing_analog, STYPE_ANALOG, "mega-001"))

# DHT11 and DHT22 are different types — each gets its own limit
existing_dht11 = [{'type': STYPE_DHT11, 'node_id': 'mega-001', 'active': True}]
check("can add DHT22 alongside DHT11",
      can_add_sensor(existing_dht11, STYPE_DHT22, "mega-001"))

# DS18B20 (LIMIT_MANY) — can add many
ds_sensors = [{'type': STYPE_DS18B20, 'node_id': 'mega-001', 'active': True} for _ in range(15)]
check("can add 16th DS18B20",
      can_add_sensor(ds_sensors, STYPE_DS18B20, "mega-001"))

ds_full = [{'type': STYPE_DS18B20, 'node_id': 'mega-001', 'active': True} for _ in range(16)]
check("cannot add 17th DS18B20",
      not can_add_sensor(ds_full, STYPE_DS18B20, "mega-001"))

# Inactive sensor should not count
inactive_dht = [{'type': STYPE_DHT11, 'node_id': 'mega-001', 'active': False}]
check("inactive sensor does not block",
      can_add_sensor(inactive_dht, STYPE_DHT11, "mega-001"))

# --- Phase 8: expanded catalogue tests ---

# New LIMIT_MANY types can add multiple instances
check("can add voltage sensor to empty node",
      can_add_sensor([], STYPE_VOLTAGE, "mega-001"))

multi_voltage = [{'type': STYPE_VOLTAGE, 'node_id': 'mega-001', 'active': True} for _ in range(3)]
check("can add 4th voltage sensor",
      can_add_sensor(multi_voltage, STYPE_VOLTAGE, "mega-001"))

# Pressure is LIMIT_ONE
check("can add pressure to empty node",
      can_add_sensor([], STYPE_PRESSURE, "mega-001"))
existing_pressure = [{'type': STYPE_PRESSURE, 'node_id': 'mega-001', 'active': True}]
check("cannot add second pressure",
      not can_add_sensor(existing_pressure, STYPE_PRESSURE, "mega-001"))

# Contact sensors — many per node
contacts = [{'type': STYPE_CONTACT, 'node_id': 'mega-001', 'active': True} for _ in range(8)]
check("can add 9th contact sensor",
      can_add_sensor(contacts, STYPE_CONTACT, "mega-001"))

# Soil moisture — many per node
check("can add soil sensor to empty node",
      can_add_sensor([], STYPE_SOIL, "mega-001"))

# Flow sensor — many per node, needs interrupt pin
check("can add flow sensor to empty node",
      can_add_sensor([], STYPE_FLOW, "mega-001"))

# Supported vs unsupported distinction
check("DHT11 is supported", DRIVER_SUPPORTED[STYPE_DHT11])
check("DHT22 is supported", DRIVER_SUPPORTED[STYPE_DHT22])
check("Analog is supported", DRIVER_SUPPORTED[STYPE_ANALOG])
check("BME280 is not supported", not DRIVER_SUPPORTED[STYPE_BME280])
check("DS18B20 is not supported", not DRIVER_SUPPORTED[STYPE_DS18B20])
check("Voltage is not supported", not DRIVER_SUPPORTED[STYPE_VOLTAGE])
check("Current is not supported", not DRIVER_SUPPORTED[STYPE_CURRENT])
check("Water level is not supported", not DRIVER_SUPPORTED[STYPE_WATER_LVL])
check("Pressure is not supported", not DRIVER_SUPPORTED[STYPE_PRESSURE])
check("Soil is not supported", not DRIVER_SUPPORTED[STYPE_SOIL])
check("Flow is not supported", not DRIVER_SUPPORTED[STYPE_FLOW])
check("Contact is not supported", not DRIVER_SUPPORTED[STYPE_CONTACT])

# All defined types have a limit entry
check("all types have limits",
      all(t in DRIVER_LIMITS for t in DRIVER_SUPPORTED))

# All defined types have a supported entry
check("all types have supported flag",
      all(t in DRIVER_SUPPORTED for t in DRIVER_LIMITS))

# Limit values are sensible
for stype, limit in DRIVER_LIMITS.items():
    check(f"type {stype} limit > 0", limit > 0)


# =====================================================================
#  Quality name mapping
# =====================================================================
print("\n== Quality names ==")

QUALITY_NAMES = {MQ_FRESH: 'fresh', MQ_STALE: 'stale', MQ_INVALID: 'invalid', MQ_MISSING: 'missing'}

for q, name in QUALITY_NAMES.items():
    check(f"quality {q} = '{name}'", QUALITY_NAMES[q] == name)

# Worst-of-two quality (used for DHT: temperature + humidity)
def worst_quality(a, b):
    return max(a, b)

check("worst(fresh, stale) = stale",
      worst_quality(MQ_FRESH, MQ_STALE) == MQ_STALE)
check("worst(stale, invalid) = invalid",
      worst_quality(MQ_STALE, MQ_INVALID) == MQ_INVALID)
check("worst(fresh, fresh) = fresh",
      worst_quality(MQ_FRESH, MQ_FRESH) == MQ_FRESH)


# =====================================================================
#  Sensor driver metadata completeness
# =====================================================================
print("\n== Driver metadata ==")

DRIVERS = [
    {'type': STYPE_DHT11,   'name': 'DHT11',   'quantity': 'temperature+humidity', 'unit': '°C', 'min': 0, 'max': 500, 'limit': LIMIT_ONE},
    {'type': STYPE_DHT22,   'name': 'DHT22',   'quantity': 'temperature+humidity', 'unit': '°C', 'min': -400, 'max': 800, 'limit': LIMIT_ONE},
    {'type': STYPE_BME280,  'name': 'BME280',  'quantity': 'temperature+humidity+pressure', 'unit': '°C', 'min': -400, 'max': 850, 'limit': LIMIT_ONE},
    {'type': STYPE_DS18B20, 'name': 'DS18B20', 'quantity': 'temperature', 'unit': '°C', 'min': -550, 'max': 1250, 'limit': LIMIT_MANY},
    {'type': STYPE_ANALOG,  'name': 'Analog sensor', 'quantity': 'analog', 'unit': '%', 'min': 0, 'max': 1000, 'limit': LIMIT_ONE},
]

for d in DRIVERS:
    check(f"{d['name']} has quantity", len(d['quantity']) > 0)
    check(f"{d['name']} has unit", len(d['unit']) > 0)
    check(f"{d['name']} range valid", d['min'] < d['max'])
    check(f"{d['name']} limit > 0", d['limit'] > 0)


# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*40}")
print(f"  {passed} passed, {failed} failed")
print(f"{'='*40}")
sys.exit(1 if failed else 0)
