#!/usr/bin/env python3
"""
Phase 37 tests: Config migration framework.
"""
import sys
import os
import re

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

GW = os.path.join(os.path.dirname(__file__), '..', 'firmware', 'gateway-esp32')

# =====================================================================
#  config.h: Schema version bump and migration method
# =====================================================================
print("\n== Phase 37: Config schema version ==")

with open(os.path.join(GW, 'config.h'), 'r', encoding='utf-8') as f:
    config_h = f.read()

check("CONFIG_SCHEMA_VERSION bumped to 2",
      'CONFIG_SCHEMA_VERSION = 2' in config_h)

print("\n== Phase 37: migrateConfig method ==")

check("migrateConfig method exists",
      'bool migrateConfig()' in config_h)
check("Reads _v from each config file",
      'doc["_v"]' in config_h and 'fileVer' in config_h)
check("Compares fileVer to CONFIG_SCHEMA_VERSION",
      'fileVer >= CONFIG_SCHEMA_VERSION' in config_h or 'fileVer < 2' in config_h)
check("v1→v2: adds interlock default to equipment",
      'interlock' in config_h and 'fileVer < 2' in config_h)
check("Updates _v to CONFIG_SCHEMA_VERSION",
      'doc["_v"] = CONFIG_SCHEMA_VERSION' in config_h)
check("Writes migrated file back",
      'LittleFS.open(files[i], "w")' in config_h)
check("Reloads config after migration",
      'loadEquipment()' in config_h and 'migrateConfig' in config_h)
check("Iterates over all config files",
      '/config/env.json' in config_h and '/config/equipment.json' in config_h and
      '/config/rules.json' in config_h)
check("Returns true when migration performed",
      'return migrated' in config_h)

# =====================================================================
#  gateway-esp32.ino: Migration called in setup and restore
# =====================================================================
print("\n== Phase 37: Migration integration ==")

with open(os.path.join(GW, 'gateway-esp32.ino'), 'r', encoding='utf-8') as f:
    ino = f.read()

check("migrateConfig called in setup after validateConfig",
      'config.migrateConfig()' in ino)

# Verify order: validateConfig before migrateConfig in setup()
setup_section = ino[ino.find('void setup()'):] if 'void setup()' in ino else ''
validate_idx = setup_section.find('config.validateConfig()')
migrate_idx = setup_section.find('config.migrateConfig()')
check("migrateConfig called after validateConfig in setup",
      validate_idx >= 0 and migrate_idx >= 0 and migrate_idx > validate_idx)

check("Migration logged to eventLog",
      'migrated to schema' in ino.lower() or 'schema v2' in ino)
check("Migration logged to Serial",
      'Migrated config' in ino)

# Check migrateConfig called in restore handler
restore_section = ino[ino.find('handleConfigRestore'):] if 'handleConfigRestore' in ino else ''
check("migrateConfig called after config restore",
      'migrateConfig()' in restore_section)

# =====================================================================
#  config.h: All save methods use CONFIG_SCHEMA_VERSION
# =====================================================================
print("\n== Phase 37: Schema version in save methods ==")

# Count occurrences of _v = CONFIG_SCHEMA_VERSION in config.h
save_version_count = config_h.count('doc["_v"] = CONFIG_SCHEMA_VERSION')
# Should be in save methods (env, zones, equipment, sensors, rules) = 5, plus migration
check(f"_v = CONFIG_SCHEMA_VERSION in save methods ({save_version_count} found)",
      save_version_count >= 5)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 37 results: {passed} passed, {failed} failed")
if failed:
    print("SOME TESTS FAILED")
    sys.exit(1)
else:
    print("ALL TESTS PASSED")
