#!/usr/bin/env python3
"""
Phase 27 tests: node health dashboard.
Verifies the new Nodes section on the dashboard with per-node health cards,
RSSI display, online/offline status, and supporting CSS styles.
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

BASE = os.path.join(os.path.dirname(__file__), '..', 'firmware', 'gateway-esp32', 'data')

# =====================================================================
#  HTML: Nodes section on dashboard
# =====================================================================
print("\n== HTML: Nodes section ==")

html_path = os.path.join(BASE, 'index.html')
with open(html_path, 'r', encoding='utf-8') as f:
    html = f.read()

check("sect-nodes wrapper exists", 'id="sect-nodes"' in html)
check("sect-nodes has section header", 'data-sect="sect-nodes"' in html)
check("node-health-grid container", 'id="node-health-grid"' in html)
check("nodes section before devices section", html.index('sect-nodes') < html.index('sect-devices'))

# =====================================================================
#  JS: Node health grid functions
# =====================================================================
print("\n== JS: Node health functions ==")

js_path = os.path.join(BASE, 'js', 'app.js')
with open(js_path, 'r', encoding='utf-8') as f:
    js = f.read()

check("loadNodeHealth function", 'function loadNodeHealth' in js)
check("drawNodeHealthGrid function", 'function drawNodeHealthGrid' in js)
check("fetches /api/hw/nodes", "fetch('/api/hw/nodes')" in js)
check("renders nh-card class", "nh-card" in js)
check("shows online dot", "'nh-dot online'" in js)
check("shows offline dot", "'nh-dot offline'" in js)
check("displays RSSI value", "n.rssi" in js)
check("displays node address", "n.address.toString(16)" in js)
check("displays node board", "n.board" in js)
check("displays sensor count", "n.sensors" in js)
check("called from poll cycle", "loadNodeHealth()" in js)
check("uses signalLabel for RSSI", "signalLabel(n.rssi)" in js)

# =====================================================================
#  CSS: Node health styles
# =====================================================================
print("\n== CSS: Node health styles ==")

css_path = os.path.join(BASE, 'css', 'style.css')
with open(css_path, 'r', encoding='utf-8') as f:
    css = f.read()

check("node-health-grid class", '.node-health-grid' in css)
check("nh-card class", '.nh-card' in css)
check("nh-offline class", '.nh-offline' in css)
check("nh-header class", '.nh-header' in css)
check("nh-dot class", '.nh-dot' in css)
check("nh-dot online variant", '.nh-dot.online' in css)
check("nh-dot offline variant", '.nh-dot.offline' in css)
check("nh-name class", '.nh-name' in css)
check("nh-status class", '.nh-status' in css)
check("nh-body class", '.nh-body' in css)
check("nh-stat class", '.nh-stat' in css)
check("responsive grid", 'grid-template-columns: 1fr 1fr' in css)
check("online border uses --on", '--on' in css)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 27 results: {passed} passed, {failed} failed")
if failed:
    print("SOME TESTS FAILED")
    sys.exit(1)
else:
    print("ALL TESTS PASSED")
