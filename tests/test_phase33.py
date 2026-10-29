#!/usr/bin/env python3
"""
Phase 33 tests: Relay interlocks and exponential backoff.
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
DATA = os.path.join(GW, 'data')

# =====================================================================
#  config.h: Equipment struct has interlockGroup field
# =====================================================================
print("\n== Phase 33: Equipment interlock struct ==")

with open(os.path.join(GW, 'config.h'), 'r', encoding='utf-8') as f:
    config_h = f.read()

check("Equipment struct has interlockGroup field",
      'interlockGroup' in config_h and 'uint8_t' in config_h.split('interlockGroup')[0].split('\n')[-1])
check("interlockGroup loaded from JSON (interlock key)",
      '"interlock"' in config_h and 'interlockGroup' in config_h)
check("interlockGroup saved to JSON",
      'obj["interlock"]' in config_h and 'interlockGroup' in config_h)
check("addEquipment accepts interlock parameter",
      'uint8_t interlock' in config_h and 'addEquipment' in config_h)
check("updateEquipment accepts interlock parameter",
      'updateEquipment' in config_h and 'interlock' in config_h)
check("interlockGroup assigned in addEquipment",
      'eq.interlockGroup = interlock' in config_h)
check("interlockGroup assigned in updateEquipment",
      'eq->interlockGroup = interlock' in config_h)

# =====================================================================
#  device.h: OutputState has retryAfter field
# =====================================================================
print("\n== Phase 33: OutputState backoff field ==")

with open(os.path.join(GW, 'device.h'), 'r', encoding='utf-8') as f:
    device_h = f.read()

check("OutputState has retryAfter field",
      'retryAfter' in device_h and 'uint32_t' in device_h.split('retryAfter')[0].split('\n')[-1])

# =====================================================================
#  gateway-esp32.ino: Interlock enforcement
# =====================================================================
print("\n== Phase 33: Interlock enforcement in gateway ==")

with open(os.path.join(GW, 'gateway-esp32.ino'), 'r', encoding='utf-8') as f:
    ino = f.read()

check("interlockConflict helper function exists",
      'interlockConflict' in ino and 'const char *interlockConflict' in ino)
check("interlockConflict checks interlockGroup == 0",
      'interlockGroup == 0' in ino)
check("interlockConflict checks confirmedOn",
      'confirmedOn' in ino and 'interlockConflict' in ino)
check("handleSetDevice calls interlockConflict",
      'interlockConflict(eq->outputIndex)' in ino)
check("handleSetDevice returns 409 on interlock conflict",
      '"interlock:' in ino)
check("evaluateRules checks interlock before SCHED_ON",
      'INTERLOCK' in ino and 'SCHED_ON' in ino)
check("evaluateRules checks interlock before TRIGGER",
      'INTERLOCK' in ino and 'TRIGGER' in ino)
check("INTERLOCK logged to actionLog",
      '"INTERLOCK"' in ino)
check("handleAddEquipment parses interlock field",
      'doc["interlock"]' in ino)

# =====================================================================
#  gateway-esp32.ino: Exponential backoff
# =====================================================================
print("\n== Phase 33: Exponential backoff ==")

check("Backoff calculation uses bit shift",
      '1 << o.attempt' in ino or '1 << o.attempt)' in ino)
check("retryAfter set in timeout branch",
      'o.retryAfter = millis()' in ino)
check("retryAfter checked in CMD_PENDING branch",
      'o.retryAfter' in ino and 'CMD_PENDING' in ino)
check("Status set back to CMD_PENDING for retry",
      'o.status = CMD_PENDING' in ino)
check("Fresh command resets attempt to 0",
      'o.attempt = 0' in ino and 'retryAfter == 0' in ino)
check("retryAfter cleared in sendOutputCmd",
      'o.retryAfter  = 0' in ino and 'sendOutputCmd' in ino)

# =====================================================================
#  Dashboard: Interlock UI
# =====================================================================
print("\n== Phase 33: Dashboard interlock UI ==")

with open(os.path.join(DATA, 'index.html'), 'r', encoding='utf-8') as f:
    html = f.read()

with open(os.path.join(DATA, 'js', 'app.js'), 'r', encoding='utf-8') as f:
    js = f.read()

check("eq-interlock select in HTML",
      'id="eq-interlock"' in html)
check("Interlock group options (1-8) in HTML",
      'Group 1' in html and 'Group 8' in html)
check("JS reads eq-interlock value",
      'eq-interlock' in js)
check("JS sends interlock in equipment data",
      'interlock:' in js and 'eq-interlock' in js)
check("setDevice handles non-ok responses with error toast",
      'res.ok' in js and 'j.error' in js)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 33 results: {passed} passed, {failed} failed")
if failed:
    print("SOME TESTS FAILED")
    sys.exit(1)
else:
    print("ALL TESTS PASSED")
