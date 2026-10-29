#!/usr/bin/env python3
"""
Phase 38 tests: Dashboard polish + documentation update.
"""
import sys
import os

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
NODE = os.path.join(os.path.dirname(__file__), '..', 'firmware', 'node-mega')
ROOT = os.path.join(os.path.dirname(__file__), '..')

# =====================================================================
#  CSS: Tablet breakpoint
# =====================================================================
print("\n== Phase 38: Tablet breakpoint ==")

with open(os.path.join(GW, 'data', 'css', 'style.css'), 'r', encoding='utf-8') as f:
    css = f.read()

check("768px media query exists",
      'min-width: 768px' in css or 'min-width:768px' in css)
check("Equipment list 2-column grid",
      'equipment-list' in css and 'grid-template-columns' in css and '1fr 1fr' in css)
check("Node health grid 2 columns at tablet",
      'node-health-grid' in css and '1fr 1fr' in css)
check("Chart height increased at tablet",
      'chart-canvas' in css and '220px' in css)
check("Settings section max-width at tablet",
      'settings-section' in css and 'max-width' in css and '640px' in css)

# =====================================================================
#  Version strings
# =====================================================================
print("\n== Phase 38: Version strings ==")

with open(os.path.join(GW, 'gateway-esp32.ino'), 'r', encoding='utf-8') as f:
    gw_ino = f.read()

check("Gateway firmware string is 3.1.0",
      '"3.1.0"' in gw_ino)
check("Gateway boot banner is v3.1.0",
      'LoRaSense Gateway v3.1.0' in gw_ino)

with open(os.path.join(NODE, 'node-mega.ino'), 'r', encoding='utf-8') as f:
    node_ino = f.read()

check("Node boot banner is v3.1.0",
      'LoRaSense Node v3.1.0' in node_ino)

# =====================================================================
#  README updates
# =====================================================================
print("\n== Phase 38: README documentation ==")

with open(os.path.join(ROOT, 'README.md'), 'r', encoding='utf-8') as f:
    readme = f.read()

check("README shows Phase 38",
      'Phase%2038' in readme or 'Phase 38' in readme)
check("README shows v3.1.0 badge",
      'v3.1.0' in readme)
check("README lists relay interlocks feature",
      'interlock' in readme.lower())
check("README lists config validation feature",
      'config validation' in readme.lower() or 'Config validation' in readme)
check("README lists rate limiting feature",
      'rate limit' in readme.lower())
check("README lists factory reset feature",
      'factory reset' in readme.lower() or 'Factory reset' in readme)
check("README lists history persistence feature",
      'history persistence' in readme.lower() or 'History persistence' in readme)
check("README lists config migration feature",
      'config migration' in readme.lower() or 'Config migration' in readme)
check("README roadmap marks multi-node done",
      '[x] Multi-node' in readme)
check("README multi-node limitation updated",
      'up to 4' in readme.lower() and 'single node' not in readme.lower())

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 38 results: {passed} passed, {failed} failed")
if failed:
    print("SOME TESTS FAILED")
    sys.exit(1)
else:
    print("ALL TESTS PASSED")
