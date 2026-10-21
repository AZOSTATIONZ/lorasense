#!/usr/bin/env python3
"""
Phase 24 tests: user documentation page.
Covers HTML docs view, CSS docs styles, JS navigation and helper functions.
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
#  HTML — Docs view structure
# =====================================================================
print("\n== HTML: Documentation view ==")

html_path = os.path.join(BASE, 'index.html')
with open(html_path, 'r', encoding='utf-8') as f:
    html = f.read()

# Help button in header
check("help-btn exists", 'id="help-btn"' in html)
check("help-btn has aria-label", 'aria-label="Help and documentation"' in html)
check("help-btn has question mark SVG", 'M9.09 9a3 3 0' in html)

# Docs view container
check("view-docs container exists", 'id="view-docs"' in html)
check("view-docs initially hidden", 'id="view-docs" style="display:none"' in html)

# Docs header
check("Help & Documentation heading", 'Help &amp; Documentation' in html)
check("docs-back-btn exists", 'id="docs-back-btn"' in html)

# Documentation sections
SECTIONS = [
    'Getting Started',
    'Dashboard',
    'Settings',
    'Hardware Studio',
    'Wiring Guide',
    'Automation Rules',
    'Sensor Catalogue',
    'Troubleshooting'
]
for section in SECTIONS:
    check(f"Section: {section}", section in html)

# Docs structure elements
check("docs-group details element", 'class="docs-group"' in html)
check("docs-heading summary element", 'class="docs-heading"' in html)
check("docs-body content div", 'class="docs-body"' in html)
check("docs-steps ordered list", 'class="docs-steps"' in html)
check("docs-callout tip box", 'class="docs-callout"' in html)
check("docs-table wiring table", 'class="docs-table"' in html)
check("docs-faq definition list", 'class="docs-faq"' in html)

# Getting Started steps content
check("Step: Power on", 'Power on' in html)
check("Step: Connect to Wi-Fi", 'Connect to Wi-Fi' in html)
check("Step: Run the wizard", 'Run the wizard' in html)
check("Step: Monitor", '<li><strong>Monitor</strong>' in html)

# Wiring tables
check("ESP32 SPI wiring (GPIO 23)", 'GPIO 23' in html)
check("Mega relay pins 22-29", '22–29' in html or '22-29' in html)
check("DHT pin 7 wiring", 'pin 7' in html)

# Sensor catalogue
check("DHT11 in catalogue", '<td>DHT11</td>' in html)
check("DHT22 in catalogue", 'DHT22' in html)
check("Analog Input in catalogue", 'Analog Input' in html)
check("BME280 in catalogue", 'BME280' in html)
check("DS18B20 in catalogue", 'DS18B20' in html)
check("docs-supported class", 'class="docs-supported"' in html)
check("docs-engineering class", 'class="docs-engineering"' in html)
check("Supported vs Engineering explanation", 'driver is built in and tested' in html)

# Troubleshooting FAQ
check("FAQ: Cannot connect Wi-Fi", 'Cannot connect to LoRaSense Wi-Fi' in html)
check("FAQ: Node offline", 'Dashboard shows' in html)
check("FAQ: Awaiting data", 'Awaiting data' in html)
check("FAQ: Relay not switching", 'Relay does not switch' in html)
check("FAQ: Fail-safe", 'All relays turned off unexpectedly' in html)
check("FAQ: Dark mode", 'Dark mode not working' in html)

# Footer
check("docs-footer section", 'class="docs-section docs-footer"' in html)
check("Offline-first message", 'No cloud. No account required.' in html)

# Presets hint placeholder
check("docs-presets-hint placeholder", 'id="docs-presets-hint"' in html)

# Placement: docs view is inside main
main_end = html.rindex('</main>')
docs_pos = html.index('id="view-docs"')
check("view-docs inside main", docs_pos < main_end)

# Help button before settings button
help_pos = html.index('id="help-btn"')
settings_pos = html.index('id="settings-btn"')
check("help-btn before settings-btn", help_pos < settings_pos)

# =====================================================================
#  CSS — Docs styles
# =====================================================================
print("\n== CSS: Documentation styles ==")

css_path = os.path.join(BASE, 'css', 'style.css')
with open(css_path, 'r', encoding='utf-8') as f:
    css = f.read()

check("Phase 24 comment exists", '/* Phase 24: User documentation page */' in css)

check(".docs-section class", '.docs-section' in css)
check(".docs-group class", '.docs-group' in css)

group_css = css[css.index('.docs-group {'):css.index('.docs-group {') + 200]
check("docs-group uses card background", 'var(--card)' in group_css)
check("docs-group has border", 'var(--border)' in group_css)

check(".docs-heading class", '.docs-heading' in css)
heading_css = css[css.index('.docs-heading {'):css.index('.docs-heading {') + 200]
check("docs-heading has cursor pointer", 'cursor: pointer' in heading_css)

check(".docs-body class", '.docs-body' in css)
check(".docs-steps class", '.docs-steps' in css)

# Step counter
check("docs-steps counter-reset", 'counter-reset: step' in css)
check("docs-steps li counter-increment", 'counter-increment: step' in css)
check("Step number styled as circle", 'border-radius: 50%' in css)

check(".docs-callout class", '.docs-callout' in css)
callout_css = css[css.index('.docs-callout {'):css.index('.docs-callout {') + 200]
check("callout uses accent-soft bg", 'var(--accent-soft)' in callout_css)
check("callout has left border", 'border-left:' in callout_css)

check(".docs-table class", '.docs-table' in css)
check(".docs-supported class", '.docs-supported' in css)
check(".docs-engineering class", '.docs-engineering' in css)

supported_css = css[css.index('.docs-supported'):css.index('.docs-supported') + 100]
check("supported uses --on color", 'var(--on)' in supported_css)

engineering_css = css[css.index('.docs-engineering'):css.index('.docs-engineering') + 100]
check("engineering uses --pending color", 'var(--pending)' in engineering_css)

check(".docs-faq dt class", '.docs-faq dt' in css)
check(".docs-faq dd class", '.docs-faq dd' in css)
check(".docs-footer class", '.docs-footer' in css)

# Arrow rotation on details open
check("Arrow before pseudo-element", "::before" in css and "'\\25B6'" in css)
check("Arrow rotates on open", 'rotate(90deg)' in css)

# =====================================================================
#  JS — Navigation and helpers
# =====================================================================
print("\n== JS: Docs navigation and helpers ==")

js_path = os.path.join(BASE, 'js', 'app.js')
with open(js_path, 'r', encoding='utf-8') as f:
    js = f.read()

# docs in views array
check("'docs' in views array", "'dashboard', 'settings', 'studio', 'docs'" in js)

# help-btn onclick
check("help-btn onclick wired", "document.getElementById('help-btn').onclick" in js)
check("help-btn shows docs view", "showView('docs')" in js)

# docs-back-btn onclick
check("docs-back-btn onclick wired", "document.getElementById('docs-back-btn').onclick" in js)

# docs view handler in showView
check("showView handles docs", "view === 'docs'" in js)
check("docs stops poll timer", js.count('clearInterval(pollTimer)') >= 3)

# populateDocsPresets function
check("populateDocsPresets function exists", 'function populateDocsPresets()' in js)
fn_start = js.index('function populateDocsPresets()')
fn_block = js[fn_start:fn_start + 500]
check("populateDocsPresets accesses docs-presets-hint", "'docs-presets-hint'" in fn_block)
check("populateDocsPresets reads currentProfile.presets", 'currentProfile.presets' in fn_block)
check("populateDocsPresets shows preset names", '.name' in fn_block)
check("populateDocsPresets called from showView", "populateDocsPresets()" in js)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 24 tests: {passed} passed, {failed} failed out of {passed + failed}")
print(f"{'='*60}")

if failed > 0:
    sys.exit(1)
