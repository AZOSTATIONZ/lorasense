#!/usr/bin/env python3
"""
Phase 36 tests: History enhancement — analog data + flash persistence.
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
#  history.h: SensorReading struct + persistence
# =====================================================================
print("\n== Phase 36: SensorReading struct ==")

with open(os.path.join(GW, 'history.h'), 'r', encoding='utf-8') as f:
    history_h = f.read()

check("SensorReading has analogX10 field",
      'analogX10' in history_h and 'int16_t' in history_h.split('analogX10')[0].split('\n')[-1])
check("push() accepts analogVal parameter",
      'int16_t analogVal' in history_h)
check("push() defaults analogVal to -1",
      'analogVal = -1' in history_h)
check("push() stores analogX10",
      'analogX10 = analogVal' in history_h or 'analogX10  = analogVal' in history_h)

print("\n== Phase 36: History persistence ==")

check("HISTORY_FILE constant defined",
      'HISTORY_FILE' in history_h and 'history.bin' in history_h)
check("HISTORY_VERSION constant defined",
      'HISTORY_VERSION' in history_h)
check("saveToFile method exists",
      'bool saveToFile()' in history_h)
check("loadFromFile method exists",
      'bool loadFromFile()' in history_h)
check("Binary header written ('H','S')",
      "'H'" in history_h and "'S'" in history_h)
check("Header includes version byte",
      'HISTORY_VERSION' in history_h)
check("Header includes entry size byte",
      'sizeof(SensorReading)' in history_h)
check("saveToFile writes head and count",
      'head' in history_h and 'count' in history_h and 'meta' in history_h)
check("loadFromFile validates header",
      "header[0] != 'H'" in history_h and "header[2] != HISTORY_VERSION" in history_h)
check("loadFromFile constrains head and count",
      'constrain(meta[0]' in history_h and 'constrain(meta[1]' in history_h)
check("lastFlush field for flush timing",
      'lastFlush' in history_h and 'uint32_t' in history_h.split('lastFlush')[0].split('\n')[-1])

# =====================================================================
#  gateway-esp32.ino: Analog in history push and API
# =====================================================================
print("\n== Phase 36: Gateway integration ==")

with open(os.path.join(GW, 'gateway-esp32.ino'), 'r', encoding='utf-8') as f:
    ino = f.read()

check("history.push passes analogValue",
      'pn->analogValue' in ino and 'history.push' in ino)
check("handleHistory includes analog field",
      'analog' in ino and 'analogX10' in ino and 'handleHistory' in ino)
check("Analog only included when >= 0",
      'analogX10 >= 0' in ino)
check("history.loadFromFile called in setup",
      'history.loadFromFile()' in ino)
check("Load restored count logged",
      'Restored' in ino and 'readings' in ino)
check("HISTORY_FLUSH_MS constant defined",
      'HISTORY_FLUSH_MS' in ino)
check("Periodic flush in loop",
      'history.saveToFile()' in ino and 'HISTORY_FLUSH_MS' in ino)
check("Factory reset includes history.bin",
      'history.bin' in ino)

# =====================================================================
#  Dashboard: Analog chart trace
# =====================================================================
print("\n== Phase 36: Dashboard analog chart ==")

with open(os.path.join(DATA, 'js', 'app.js'), 'r', encoding='utf-8') as f:
    js = f.read()

check("Chart checks for analog data presence",
      'hasAnalog' in js)
check("Analog line drawn in green (#22c55e)",
      '#22c55e' in js)
check("Analog line uses dashed style",
      'setLineDash' in js)
check("Analog bounds computed independently",
      'aMin' in js and 'aMax' in js)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 36 results: {passed} passed, {failed} failed")
if failed:
    print("SOME TESTS FAILED")
    sys.exit(1)
else:
    print("ALL TESTS PASSED")
