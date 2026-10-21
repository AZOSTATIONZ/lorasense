#!/usr/bin/env python3
"""
Phase 25 tests: sensor catalogue clarity.
Covers improved driver list UI (legend, chips, progress bar, status column),
updated docs sensor catalogue (13 sensors, legend, bus chips), and CSS styles.
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
#  JS — Enhanced loadDrivers
# =====================================================================
print("\n== JS: Enhanced loadDrivers ==")

js_path = os.path.join(BASE, 'js', 'app.js')
with open(js_path, 'r', encoding='utf-8') as f:
    js = f.read()

fn_start = js.index('function loadDrivers()')
fn_end = js.index('\n}', fn_start) + 2
fn_block = js[fn_start:fn_end]

check("loadDrivers has status legend", 'driver-status-legend' in fn_block)
check("Legend explains Supported", 'Supported' in fn_block)
check("Legend explains Engineering", 'Engineering' in fn_block)
check("Legend has ok dot", 'legend-dot-ok' in fn_block)
check("Legend has eng dot", 'legend-dot-eng' in fn_block)

check("Section label shows count for supported", "supported.length + ')" in fn_block)
check("Section label shows count for planned", "planned.length + ')" in fn_block)
check("Section uses check icon for supported", "svgIcon('check')" in fn_block)
check("Section uses clock icon for planned", "svgIcon('clock')" in fn_block)

check("Progress bar rendered", 'driver-summary-bar' in fn_block)
check("Progress fill with percentage", 'driver-progress-fill' in fn_block)
check("Progress calculates ratio", 'supported.length / drivers.length' in fn_block)

# =====================================================================
#  JS — Enhanced renderDriverCard
# =====================================================================
print("\n== JS: Enhanced renderDriverCard ==")

fn_start = js.index('function renderDriverCard(d)')
fn_end = js.index('\n}', fn_start) + 2
card_fn = js[fn_start:fn_end]

check("renderDriverCard has chips container", 'driver-chips' in card_fn)
check("Chip shows bus type", 'chip-bus' in card_fn)
check("Chip shows pin count", "pin_count" in card_fn)
check("Chip shows range", 'range_min' in card_fn)
check("Chip shows multi-instance", 'Multi-instance' in card_fn)
check("Chip shows firmware version", 'chip-fw' in card_fn)

check("Requirements has heading", "What's needed" in card_fn or "What\\'s needed" in card_fn)
check("Requirements uses board icon", "svgIcon('board')" in card_fn)
check("Requirements uses code icon", "svgIcon('code')" in card_fn)
check("Supported card shows hw note with info icon", "svgIcon('info')" in card_fn)

check("Status column container", 'driver-status-col' in card_fn)
check("Status sub-label for supported", "'Ready to use'" in card_fn)
check("Status sub-label for engineering", "'Not yet built'" in card_fn)
check("driver-status-sub class", 'driver-status-sub' in card_fn)

# =====================================================================
#  CSS — Phase 25 styles
# =====================================================================
print("\n== CSS: Phase 25 styles ==")

css_path = os.path.join(BASE, 'css', 'style.css')
with open(css_path, 'r', encoding='utf-8') as f:
    css = f.read()

check("Phase 25 comment exists", '/* Phase 25: Sensor catalogue clarity */' in css)

check(".driver-status-legend class", '.driver-status-legend' in css)
legend_css = css[css.index('.driver-status-legend'):css.index('.driver-status-legend') + 200]
check("Legend uses card background", 'var(--card)' in legend_css)

check(".legend-item class", '.legend-item' in css)
check(".legend-dot class", '.legend-dot' in css)
check(".legend-dot-ok class", '.legend-dot-ok' in css)
check(".legend-dot-eng class", '.legend-dot-eng' in css)

legend_ok = css[css.index('.legend-dot-ok'):css.index('.legend-dot-ok') + 60]
check("legend-dot-ok uses --on color", 'var(--on)' in legend_ok)
legend_eng = css[css.index('.legend-dot-eng'):css.index('.legend-dot-eng') + 70]
check("legend-dot-eng uses --pending color", 'var(--pending)' in legend_eng)

check(".driver-chips class", '.driver-chips' in css)
check(".driver-chip class", '.driver-chip' in css)
chip_css = css[css.index('.driver-chip {'):css.index('.driver-chip {') + 200]
check("Chip has pill radius", '999px' in chip_css)

check(".chip-bus class", '.chip-bus' in css)
bus_css = css[css.index('.chip-bus'):css.index('.chip-bus') + 150]
check("Bus chip uses accent color", 'var(--accent)' in bus_css)

check(".chip-fw class", '.chip-fw' in css)
fw_css = css[css.index('.chip-fw'):css.index('.chip-fw') + 100]
check("FW chip uses pending color", 'var(--pending)' in fw_css)

check(".driver-status-col class", '.driver-status-col' in css)
check(".driver-status-sub class", '.driver-status-sub' in css)
check(".req-heading class", '.req-heading' in css)

check(".driver-summary-bar class", '.driver-summary-bar' in css)
check(".driver-progress class", '.driver-progress' in css)
check(".driver-progress-fill class", '.driver-progress-fill' in css)
progress_fill = css[css.index('.driver-progress-fill'):css.index('.driver-progress-fill') + 120]
check("Progress fill uses --on color", 'var(--on)' in progress_fill)

check(".docs-bus-chip class", '.docs-bus-chip' in css)
check(".docs-legend class", '.docs-legend' in css)
check(".docs-legend-item class", '.docs-legend-item' in css)
check(".docs-legend-dot class", '.docs-legend-dot' in css)
check(".docs-sensor-table class", '.docs-sensor-table' in css)

# =====================================================================
#  HTML — Updated docs sensor catalogue
# =====================================================================
print("\n== HTML: Updated docs sensor catalogue ==")

html_path = os.path.join(BASE, 'index.html')
with open(html_path, 'r', encoding='utf-8') as f:
    html = f.read()

check("docs-sensor-catalogue id", 'id="docs-sensor-catalogue"' in html)

check("Docs legend present", 'class="docs-legend"' in html)
check("Docs legend ok dot", 'Supported' in html and 'docs-legend-dot' in html)
check("Docs legend eng dot", 'Engineering' in html and 'docs-legend-dot' in html)
check("13 sensor types noted", '13 sensor types' in html)
check("3 production-ready noted", '3 are production-ready' in html)
check("10 on roadmap noted", '10 are on the roadmap' in html)

check("Table has Bus column", '<th>Bus</th>' in html)
check("Table has Range column", '<th>Range</th>' in html)

SUPPORTED_SENSORS = ['DHT11', 'DHT22', 'Analog Input']
for s in SUPPORTED_SENSORS:
    check(f"Supported: {s} in docs table", s in html)

ENGINEERING_SENSORS = [
    'BME280', 'DS18B20', 'Voltage sensor', 'Current sensor',
    'Water level', 'Pressure sensor', 'Soil moisture',
    'Flow rate', 'Digital contact'
]
for s in ENGINEERING_SENSORS:
    check(f"Engineering: {s} in docs table", s in html)

check("Bus chip GPIO present", '>GPIO</span>' in html)
check("Bus chip I2C present", '>I2C</span>' in html)
check("Bus chip Analog present", '>Analog</span>' in html)
check("docs-bus-chip class in table", 'class="docs-bus-chip"' in html)

check("docs-sensor-table class on table", 'class="docs-table docs-sensor-table"' in html)

check("DHT11 range 0-50", '0–50' in html or '0-50' in html)
check("DS18B20 range negative", '−55' in html or '-55' in html)

check("Callout explains Hardware Studio", 'Hardware Studio' in html)
check("Callout explains commission", 'commission' in html)

# Verify old inaccurate entries removed
check("SHT31 removed (not in firmware)", 'SHT31' not in html)
check("TSL2561 removed (not in firmware)", 'TSL2561' not in html)
check("MQ-135 removed (not in firmware)", 'MQ-135' not in html)

# Total sensor rows (docs-supported + docs-engineering)
supported_count = html.count('class="docs-supported"')
engineering_count = html.count('class="docs-engineering"')
check(f"3 supported rows (got {supported_count})", supported_count == 3)
check(f"9 engineering rows (got {engineering_count})", engineering_count == 9)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 25 tests: {passed} passed, {failed} failed out of {passed + failed}")
print(f"{'='*60}")

if failed > 0:
    sys.exit(1)
