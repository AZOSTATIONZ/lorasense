#!/usr/bin/env python3
"""
Phase 22 tests: dynamic relay configuration.
Covers firmware (hardware.h, device.h, config.h, gateway-esp32.ino, node-mega.ino),
HTML relay config section, CSS relay map styles, and JS relay config functions.
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

BASE = os.path.join(os.path.dirname(__file__), '..', 'firmware', 'gateway-esp32')
DATA = os.path.join(BASE, 'data')
NODE = os.path.join(os.path.dirname(__file__), '..', 'firmware', 'node-mega')

# =====================================================================
#  hardware.h — expanded relay pins
# =====================================================================
print("\n== hardware.h: Expanded relay pins ==")

hw_path = os.path.join(BASE, 'hardware.h')
with open(hw_path, 'r') as f:
    hw = f.read()

check("MEGA_RELAY_PINS has 8 entries", '22, 23, 24, 25, 26, 27, 28, 29' in hw)
check("MEGA_RELAY_COUNT = 8", 'MEGA_RELAY_COUNT = 8' in hw)
check("MEGA_RELAY_PINS is const uint8_t array", 'const uint8_t MEGA_RELAY_PINS[]' in hw)

# =====================================================================
#  device.h — MAX_OUTPUTS expanded
# =====================================================================
print("\n== device.h: MAX_OUTPUTS ==")

dev_path = os.path.join(BASE, 'device.h')
with open(dev_path, 'r') as f:
    dev = f.read()

check("MAX_OUTPUTS = 8", 'MAX_OUTPUTS = 8' in dev)

# =====================================================================
#  config.h — relayCount field
# =====================================================================
print("\n== config.h: relayCount in Environment ==")

cfg_path = os.path.join(BASE, 'config.h')
with open(cfg_path, 'r') as f:
    cfg = f.read()

check("relayCount field in struct", 'uint8_t relayCount' in cfg)
check("relayCount default is 4", 'env.relayCount = 4' in cfg)
check("loadEnvironment reads relay_count", 'relay_count' in cfg)
check("loadEnvironment constrains to MAX_OUTPUTS", 'constrain(doc["relay_count"]' in cfg)
check("saveEnvironment writes relay_count", 'doc["relay_count"] = env.relayCount' in cfg)

# =====================================================================
#  gateway-esp32.ino — API relay_count and relay_pins
# =====================================================================
print("\n== gateway-esp32.ino: API changes ==")

ino_path = os.path.join(BASE, 'gateway-esp32.ino')
with open(ino_path, 'r') as f:
    ino = f.read()

check("API returns relay_count", 'doc["relay_count"] = relayCount' in ino)
check("API returns relay_pins array", '"relay_pins"' in ino)
check("relay_pins includes pin name", 'rp["pin"]' in ino)
check("relay_pins includes assigned_to", 'rp["assigned_to"]' in ino)
check("handleSetEnvironment accepts relay_count", 'doc["relay_count"].is<int>()' in ino)
check("handleSetEnvironment constrains relay_count", 'constrain((int)doc["relay_count"]' in ino)
check("addEquipment validates against relayCount", 'config.env.relayCount' in ino)
check("getRelays uses relayCount", 'config.env.relayCount' in ino)

# =====================================================================
#  node-mega.ino — expanded relay array
# =====================================================================
print("\n== node-mega.ino: Expanded relay pins ==")

node_path = os.path.join(NODE, 'node-mega.ino')
with open(node_path, 'r') as f:
    node = f.read()

check("Node RELAY_PIN has 8 entries", '22, 23, 24, 25, 26, 27, 28, 29' in node)
check("Node RELAY_COUNT = 4 (user default)", 'RELAY_COUNT = 4' in node)
check("Node relayState[8]", 'relayState[8]' in node)
check("Node supports 1/2/4/8 comment", '1/2/4/8' in node)

# =====================================================================
#  HTML — Relay config section
# =====================================================================
print("\n== HTML: Relay config section ==")

html_path = os.path.join(DATA, 'index.html')
with open(html_path, 'r') as f:
    html = f.read()

check("relay-config-section exists", 'id="relay-config-section"' in html)
check("Relay Module heading", '<h3>Relay Module</h3>' in html)
check("relay-count-input select", 'id="relay-count-input"' in html)
check("1-channel option", 'value="1">1-channel' in html)
check("2-channel option", 'value="2">2-channel' in html)
check("4-channel option", 'value="4">4-channel' in html)
check("8-channel option", 'value="8">8-channel' in html)
check("relay-pin-map container", 'id="relay-pin-map"' in html)
check("save-relay-btn button", 'id="save-relay-btn"' in html)

# Verify section is inside settings area (after settings-btn)
settings_start = html.index('id="settings-btn"')
relay_start = html.index('id="relay-config-section"')
check("Relay section after settings button", relay_start > settings_start)

# =====================================================================
#  CSS — Relay map styles
# =====================================================================
print("\n== CSS: Relay map styles ==")

css_path = os.path.join(DATA, 'css', 'style.css')
with open(css_path, 'r') as f:
    css = f.read()

check("Phase 22 comment exists", '/* Phase 22: Relay module config */' in css)
check(".relay-map class", '.relay-map' in css)
check(".relay-map-row class", '.relay-map-row' in css)

# relay-map-row layout
row_css = css[css.index('.relay-map-row'):css.index('.relay-map-row') + 300]
check("relay-map-row uses flex", 'display: flex' in row_css)
check("relay-map-row has gap", 'gap:' in row_css)
check("relay-map-row has border-bottom", 'border-bottom:' in row_css)

check(".relay-map-idx class", '.relay-map-idx' in css)
idx_css = css[css.index('.relay-map-idx'):css.index('.relay-map-idx') + 200]
check("relay-map-idx bold", 'font-weight: 700' in idx_css)
check("relay-map-idx uses accent color", 'var(--accent)' in idx_css)

check(".relay-map-pin class", '.relay-map-pin' in css)
pin_css = css[css.index('.relay-map-pin'):css.index('.relay-map-pin') + 200]
check("relay-map-pin monospace", 'monospace' in pin_css)

check(".relay-map-assign class", '.relay-map-assign' in css)
check(".relay-map-assign.assigned class", '.relay-map-assign.assigned' in css)
assigned_css = css[css.index('.relay-map-assign.assigned'):css.index('.relay-map-assign.assigned') + 150]
check("assigned uses --on color", 'var(--on)' in assigned_css)

# =====================================================================
#  JS — Relay config functions
# =====================================================================
print("\n== JS: Relay config functions ==")

js_path = os.path.join(DATA, 'js', 'app.js')
with open(js_path, 'r') as f:
    js = f.read()

# renderRelayPinMap function
check("renderRelayPinMap function exists", 'function renderRelayPinMap(cfg)' in js)
rr_fn = js[js.index('function renderRelayPinMap(cfg)'):js.index('function renderRelayPinMap(cfg)') + 800]
check("renderRelayPinMap reads relay_pins", 'relay_pins' in rr_fn)
check("renderRelayPinMap reads relay_count", 'relay_count' in rr_fn)
check("renderRelayPinMap renders relay-map class", 'relay-map' in rr_fn)
check("renderRelayPinMap renders relay-map-row", 'relay-map-row' in rr_fn)
check("renderRelayPinMap renders relay-map-idx", 'relay-map-idx' in rr_fn)
check("renderRelayPinMap renders relay-map-pin", 'relay-map-pin' in rr_fn)
check("renderRelayPinMap renders relay-map-assign", 'relay-map-assign' in rr_fn)
check("renderRelayPinMap shows 'Available' for unassigned", "'Available'" in rr_fn)
check("renderRelayPinMap shows assigned_to", 'assigned_to' in rr_fn)
check("renderRelayPinMap uses escHtml", 'escHtml' in rr_fn)

# save-relay-btn handler
save_idx = js.index("document.getElementById('save-relay-btn')")
save_fn = js[save_idx:save_idx + 500]
check("save-relay-btn onclick exists", "onclick" in save_fn)
check("save handler reads relay-count-input", "'relay-count-input'" in save_fn)
check("save handler POSTs to /api/config/environment", "/api/config/environment" in save_fn)
check("save handler sends relay_count in body", 'relay_count' in save_fn)
check("save handler calls loadConfig after save", 'loadConfig()' in save_fn)
check("save handler shows success toast", 'showToast' in save_fn)

# relay-count-input onchange preview
onchange_idx = js.index("document.getElementById('relay-count-input').onchange")
onchange_fn = js[onchange_idx:onchange_idx + 500]
check("relay-count-input onchange exists", "onchange" in onchange_fn)
check("onchange calls renderRelayPinMap for preview", 'renderRelayPinMap(preview)' in onchange_fn)

# renderSettings populates relay config
render_settings_start = js.index('function renderSettings(')
render_relay_area = js[render_settings_start:render_settings_start + 4000]
check("renderSettings populates relay-count-input", "'relay-count-input'" in render_relay_area)
check("renderSettings calls renderRelayPinMap", 'renderRelayPinMap(cfg)' in render_relay_area)

# Equipment dropdown uses relay_count
equip_area = js[js.index('activeRelays'):js.index('activeRelays') + 200]
check("Equipment dropdown uses relay_count", 'cfg.relay_count' in equip_area)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 22 tests: {passed} passed, {failed} failed out of {passed + failed}")
print(f"{'='*60}")

if failed > 0:
    sys.exit(1)
