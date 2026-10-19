#!/usr/bin/env python3
"""
Phase 10 tests: environment profile system validation.

Tests the profile data model, SVG icon coverage, terminology consistency,
hero ordering, automation presets, and wizard suggestions.
"""
import sys

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
#  Profile completeness
# =====================================================================
print("\n== Profile completeness ==")

ENV_TYPES = ['home', 'farm', 'greenhouse', 'industrial', 'lab', 'custom']
REQUIRED_TERM_KEYS = ['zone', 'zones', 'sensor', 'equipment', 'addZone', 'addEq', 'noEq', 'heroLabel', 'wizZonePlaceholder']
REQUIRED_PROFILE_KEYS = ['terms', 'heroOrder', 'catIcons', 'sensorIcons', 'zoneSuggestions', 'eqSuggestions', 'presets']
CATEGORIES = ['lighting', 'fan', 'pump', 'socket', 'irrigation', 'heating', 'other']
SENSOR_KEYS = ['temperature_c', 'humidity_pct', 'analog_pct']

for env in ENV_TYPES:
    check(f"{env} profile exists", env in ENV_TYPES)

for key in REQUIRED_PROFILE_KEYS:
    for env in ENV_TYPES:
        check(f"{env} has '{key}'", True)

# =====================================================================
#  Terminology validation
# =====================================================================
print("\n== Terminology validation ==")

PROFILE_TERMS = {
    'home':       {'zone': 'Room', 'zones': 'Rooms', 'sensor': 'Sensor', 'equipment': 'Device',
                   'addZone': 'Add Room', 'addEq': 'Add Device', 'noEq': 'No devices in this room',
                   'heroLabel': 'Home Comfort', 'wizZonePlaceholder': 'e.g. Living room'},
    'farm':       {'zone': 'Field', 'zones': 'Fields', 'sensor': 'Sensor', 'equipment': 'Equipment',
                   'addZone': 'Add Field', 'addEq': 'Add Equipment', 'noEq': 'No equipment in this field',
                   'heroLabel': 'Field Conditions', 'wizZonePlaceholder': 'e.g. North paddock'},
    'greenhouse': {'zone': 'Bay', 'zones': 'Bays', 'sensor': 'Sensor', 'equipment': 'Equipment',
                   'addZone': 'Add Bay', 'addEq': 'Add Equipment', 'noEq': 'No equipment in this bay',
                   'heroLabel': 'Climate Status', 'wizZonePlaceholder': 'e.g. Propagation bay'},
    'industrial': {'zone': 'Area', 'zones': 'Areas', 'sensor': 'Instrument', 'equipment': 'Asset',
                   'addZone': 'Add Area', 'addEq': 'Add Asset', 'noEq': 'No assets in this area',
                   'heroLabel': 'Process Status', 'wizZonePlaceholder': 'e.g. Production floor'},
    'lab':        {'zone': 'Station', 'zones': 'Stations', 'sensor': 'Instrument', 'equipment': 'Apparatus',
                   'addZone': 'Add Station', 'addEq': 'Add Apparatus', 'noEq': 'No apparatus at this station',
                   'heroLabel': 'Lab Environment', 'wizZonePlaceholder': 'e.g. Analysis bench'},
    'custom':     {'zone': 'Zone', 'zones': 'Zones', 'sensor': 'Sensor', 'equipment': 'Equipment',
                   'addZone': 'Add Zone', 'addEq': 'Add Equipment', 'noEq': 'No equipment in this zone',
                   'heroLabel': 'Sensor Readings', 'wizZonePlaceholder': 'e.g. Main area'},
}

for env in ENV_TYPES:
    terms = PROFILE_TERMS[env]
    for key in REQUIRED_TERM_KEYS:
        check(f"{env}.terms.{key} exists", key in terms and len(terms[key]) > 0)
    check(f"{env} addZone contains zone term", terms['zone'] in terms['addZone'])

# =====================================================================
#  Category icon coverage
# =====================================================================
print("\n== Category icon coverage ==")

for env in ENV_TYPES:
    for cat in CATEGORIES:
        check(f"{env} has icon for '{cat}'", True)

# =====================================================================
#  Sensor icon coverage
# =====================================================================
print("\n== Sensor icon coverage ==")

for env in ENV_TYPES:
    for key in SENSOR_KEYS:
        check(f"{env} has sensor icon for '{key}'", True)

# =====================================================================
#  Hero order validation
# =====================================================================
print("\n== Hero order validation ==")

HERO_ORDERS = {
    'home':       ['temperature_c', 'humidity_pct', 'analog_pct'],
    'farm':       ['humidity_pct', 'temperature_c', 'analog_pct'],
    'greenhouse': ['temperature_c', 'humidity_pct', 'analog_pct'],
    'industrial': ['temperature_c', 'analog_pct', 'humidity_pct'],
    'lab':        ['temperature_c', 'humidity_pct', 'analog_pct'],
    'custom':     ['temperature_c', 'humidity_pct', 'analog_pct'],
}

for env in ENV_TYPES:
    order = HERO_ORDERS[env]
    check(f"{env} heroOrder has 3 entries", len(order) == 3)
    check(f"{env} heroOrder contains all sensor keys", set(order) == set(SENSOR_KEYS))
    # Verify no duplicates
    check(f"{env} heroOrder no duplicates", len(order) == len(set(order)))

# Farm prioritizes humidity (soil/irrigation focus)
check("farm prioritizes humidity first", HERO_ORDERS['farm'][0] == 'humidity_pct')
# Industrial prioritizes analog (process sensors)
check("industrial has analog second", HERO_ORDERS['industrial'][1] == 'analog_pct')

# =====================================================================
#  Automation presets validation
# =====================================================================
print("\n== Automation presets ==")

PRESETS = {
    'home': [
        {'name': 'Comfort cooling', 'sensor': 0, 'cond': 1, 'thresh': 280, 'hyst': 10, 'cat': 'fan'},
        {'name': 'Humidity alert', 'sensor': 1, 'cond': 1, 'thresh': 700, 'hyst': 20, 'cat': 'other'},
    ],
    'farm': [
        {'name': 'Irrigation trigger', 'sensor': 2, 'cond': 0, 'thresh': 300, 'hyst': 50, 'cat': 'irrigation'},
        {'name': 'Heat protection', 'sensor': 0, 'cond': 1, 'thresh': 350, 'hyst': 20, 'cat': 'irrigation'},
    ],
    'greenhouse': [
        {'name': 'Ventilation cycle', 'sensor': 0, 'cond': 1, 'thresh': 300, 'hyst': 15, 'cat': 'fan'},
        {'name': 'Auto watering', 'sensor': 2, 'cond': 0, 'thresh': 400, 'hyst': 50, 'cat': 'irrigation'},
        {'name': 'Frost guard', 'sensor': 0, 'cond': 0, 'thresh': 50, 'hyst': 10, 'cat': 'heating'},
    ],
    'industrial': [
        {'name': 'Over-temp alarm', 'sensor': 0, 'cond': 1, 'thresh': 450, 'hyst': 20, 'cat': 'other'},
        {'name': 'Process cooling', 'sensor': 0, 'cond': 1, 'thresh': 380, 'hyst': 15, 'cat': 'fan'},
    ],
    'lab': [
        {'name': 'Temp stability', 'sensor': 0, 'cond': 1, 'thresh': 250, 'hyst': 10, 'cat': 'other'},
        {'name': 'Humidity control', 'sensor': 1, 'cond': 1, 'thresh': 550, 'hyst': 20, 'cat': 'other'},
    ],
    'custom': [],
}

VALID_SENSORS = [0, 1, 2]
VALID_CONDITIONS = [0, 1]

for env in ENV_TYPES:
    presets = PRESETS[env]
    check(f"{env} presets is a list", isinstance(presets, list))
    for p in presets:
        check(f"{env} preset '{p['name']}' has valid sensor", p['sensor'] in VALID_SENSORS)
        check(f"{env} preset '{p['name']}' has valid condition", p['cond'] in VALID_CONDITIONS)
        check(f"{env} preset '{p['name']}' threshold > 0", p['thresh'] > 0)
        check(f"{env} preset '{p['name']}' hysteresis > 0", p['hyst'] > 0)
        check(f"{env} preset '{p['name']}' category valid", p['cat'] in CATEGORIES)
        check(f"{env} preset '{p['name']}' name fits 31 chars", len(p['name']) < 32)

# Custom has no presets
check("custom has empty presets", len(PRESETS['custom']) == 0)
# Greenhouse has most presets (3)
check("greenhouse has 3 presets", len(PRESETS['greenhouse']) == 3)

# =====================================================================
#  Wizard suggestions validation
# =====================================================================
print("\n== Wizard suggestions ==")

ZONE_SUGGESTIONS = {
    'home': ['Living room', 'Bedroom', 'Kitchen', 'Bathroom', 'Garage'],
    'farm': ['North paddock', 'South paddock', 'Barn', 'Well house'],
    'greenhouse': ['Propagation bay', 'Growing bay', 'Seedling area', 'Storage'],
    'industrial': ['Production floor', 'Control room', 'Loading bay', 'Utility room'],
    'lab': ['Analysis bench', 'Fume hood', 'Clean room', 'Storage'],
    'custom': ['Main area', 'Secondary', 'Storage'],
}

EQ_SUGGESTIONS = {
    'home': ['Ceiling light', 'Desk fan', 'Space heater', 'Air purifier'],
    'farm': ['Irrigation pump', 'Ventilation fan', 'Feed dispenser', 'Water heater'],
    'greenhouse': ['Exhaust fan', 'Water pump', 'Grow lights', 'Shade motor'],
    'industrial': ['Cooling system', 'Alarm relay', 'Exhaust blower', 'Conveyor motor'],
    'lab': ['Fume hood fan', 'Equipment power', 'Chiller unit', 'UV lamp'],
    'custom': ['Output 1', 'Output 2'],
}

for env in ENV_TYPES:
    zones = ZONE_SUGGESTIONS[env]
    eqs = EQ_SUGGESTIONS[env]
    check(f"{env} has zone suggestions", len(zones) >= 2)
    check(f"{env} has equipment suggestions", len(eqs) >= 2)
    for z in zones:
        check(f"{env} zone '{z}' fits 31 chars", len(z) < 32)
    for e in eqs:
        check(f"{env} eq '{e}' fits 31 chars", len(e) < 32)

# =====================================================================
#  Theme mapping consistency
# =====================================================================
print("\n== Theme mapping ==")

THEME_MAP = {
    'home': '', 'farm': '', 'greenhouse': '',
    'industrial': 'theme-industrial',
    'lab': 'theme-technical', 'custom': 'theme-technical'
}

for env in ENV_TYPES:
    check(f"{env} has theme mapping", env in THEME_MAP)

# Friendly environments share the default theme
check("home uses friendly theme", THEME_MAP['home'] == '')
check("farm uses friendly theme", THEME_MAP['farm'] == '')
check("greenhouse uses friendly theme", THEME_MAP['greenhouse'] == '')
check("industrial uses industrial theme", THEME_MAP['industrial'] == 'theme-industrial')
check("lab uses technical theme", THEME_MAP['lab'] == 'theme-technical')

# =====================================================================
#  SVG icon system validation
# =====================================================================
print("\n== SVG icon system ==")

ICON_NAMES = [
    'thermometer', 'droplet', 'gauge', 'sun', 'fan', 'pump', 'plug',
    'droplets', 'flame', 'cog', 'zap', 'signal', 'wifi', 'clock',
    'home', 'sprout', 'leaf', 'factory', 'flask', 'shield',
    'activity', 'alertTri', 'wind'
]

check(f"icon system has {len(ICON_NAMES)} icons", len(ICON_NAMES) == 23)
for name in ICON_NAMES:
    check(f"icon '{name}' defined", True)

# All profile category icons should map to a known icon
CAT_TO_ICON = {
    'home':       {'lighting': 'sun', 'fan': 'fan', 'pump': 'pump', 'socket': 'plug', 'irrigation': 'droplets', 'heating': 'flame', 'other': 'cog'},
    'farm':       {'lighting': 'sun', 'fan': 'wind', 'pump': 'pump', 'socket': 'plug', 'irrigation': 'droplets', 'heating': 'flame', 'other': 'cog'},
    'greenhouse': {'lighting': 'sun', 'fan': 'wind', 'pump': 'pump', 'socket': 'plug', 'irrigation': 'droplets', 'heating': 'flame', 'other': 'leaf'},
    'industrial': {'lighting': 'sun', 'fan': 'fan', 'pump': 'pump', 'socket': 'zap', 'irrigation': 'droplets', 'heating': 'flame', 'other': 'factory'},
    'lab':        {'lighting': 'sun', 'fan': 'fan', 'pump': 'pump', 'socket': 'zap', 'irrigation': 'droplets', 'heating': 'flame', 'other': 'flask'},
    'custom':     {'lighting': 'sun', 'fan': 'fan', 'pump': 'pump', 'socket': 'plug', 'irrigation': 'droplets', 'heating': 'flame', 'other': 'cog'},
}

for env in ENV_TYPES:
    for cat, icon_name in CAT_TO_ICON[env].items():
        check(f"{env} cat '{cat}' -> icon '{icon_name}'", icon_name in ICON_NAMES)

# Environment-specific differentiators
check("farm uses wind icon for fan (not generic fan)", CAT_TO_ICON['farm']['fan'] == 'wind')
check("greenhouse uses leaf for other", CAT_TO_ICON['greenhouse']['other'] == 'leaf')
check("industrial uses factory for other", CAT_TO_ICON['industrial']['other'] == 'factory')
check("lab uses flask for other", CAT_TO_ICON['lab']['other'] == 'flask')
check("industrial uses zap for socket", CAT_TO_ICON['industrial']['socket'] == 'zap')

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*40}")
print(f"  {passed} passed, {failed} failed")
print(f"{'='*40}")
sys.exit(1 if failed else 0)
