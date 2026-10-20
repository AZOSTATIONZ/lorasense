#!/usr/bin/env python3
"""
Phase 19 tests: connection health dashboard and signal visualization.
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
#  HTML structure
# =====================================================================
print("\n== HTML: Health strip container ==")

html_path = os.path.join(BASE, 'index.html')
with open(html_path, 'r') as f:
    html = f.read()

check("health-strip exists", 'id="health-strip"' in html)
check("health-strip has class", 'class="health-strip"' in html)
check("health-strip after status-bar", html.find('health-strip') > html.find('status-bar'))
check("health-strip before chart section", html.find('health-strip') < html.find('sect-chart'))

# =====================================================================
#  CSS — health strip styles
# =====================================================================
print("\n== CSS: Health strip styles ==")

css_path = os.path.join(BASE, 'css', 'style.css')
with open(css_path, 'r') as f:
    css = f.read()

check(".health-strip style", '.health-strip' in css)
strip_css = css.split('.health-strip')[1].split('}')[0]
check("health-strip uses grid", 'grid' in strip_css)
check("health-strip has gap", 'gap' in strip_css)

check(".health-card style", '.health-card' in css)
card_css = css.split('.health-card')[1].split('}')[0]
check("health-card background", 'var(--card)' in card_css)
check("health-card border", 'var(--border)' in card_css)
check("health-card flex layout", 'flex' in card_css)
check("health-card gap", 'gap' in card_css)

check(".health-icon style", '.health-icon' in css)
check(".health-icon svg sizing", '.health-icon svg' in css)
check(".health-body style", '.health-body' in css)
check(".health-label style", '.health-label' in css)
label_css = css.split('.health-label')[1].split('}')[0]
check("health-label uppercase", 'uppercase' in label_css)
check("health-label small font", 'font-size' in label_css)

check(".health-val style", '.health-val' in css)
val_css = css.split('.health-val')[1].split('}')[0]
check("health-val font-weight bold", 'font-weight' in val_css and '600' in val_css)
check("health-val tabular-nums", 'tabular-nums' in val_css)

# Signal bars
check(".signal-bars style", '.signal-bars' in css)
bars_css = css.split('.signal-bars')[1].split('}')[0]
check("signal-bars flex layout", 'flex' in bars_css)
check("signal-bars align end", 'flex-end' in bars_css)

check(".signal-bar base style", '.signal-bar' in css)
bar_css = css[css.index('.signal-bar {') if '.signal-bar {' in css else css.index('.signal-bar{'):]
bar_block = bar_css.split('}')[0]
check("signal-bar background", 'var(--border)' in bar_block)
check("signal-bar transition", 'transition' in bar_block)

check("signal-bar.active style", '.signal-bar.active' in css)
check("signal-bar.warn style", '.signal-bar.warn' in css)
check("signal-bar.fail style", '.signal-bar.fail' in css)
check("signal-bar active uses --on", 'var(--on)' in css.split('.signal-bar.active')[1].split('}')[0])
check("signal-bar warn uses --pending", 'var(--pending)' in css.split('.signal-bar.warn')[1].split('}')[0])
check("signal-bar fail uses --fail", 'var(--fail)' in css.split('.signal-bar.fail')[1].split('}')[0])

# Five bar heights
check("5 bar height levels", '.signal-bar:nth-child(5)' in css)
bar5 = css.split('.signal-bar:nth-child(5)')[1].split('}')[0]
check("5th bar tallest (16px)", '16px' in bar5)

# Connection pulse
check(".conn-pulse style", '.conn-pulse' in css)
pulse_css = css[css.index('.conn-pulse {') if '.conn-pulse {' in css else css.index('.conn-pulse{'):]
pulse_block = pulse_css.split('}')[0]
check("conn-pulse background --on", 'var(--on)' in pulse_block)
check("conn-pulse border-radius 50%", '50%' in pulse_block)
check("conn-pulse animation", 'animation' in pulse_block)

check(".conn-pulse.offline style", '.conn-pulse.offline' in css)
offline_css = css.split('.conn-pulse.offline')[1].split('}')[0]
check("offline pulse uses --fail", 'var(--fail)' in offline_css)
check("offline pulse no animation", 'none' in offline_css)

check("conn-blink keyframes", '@keyframes conn-blink' in css)

# =====================================================================
#  JavaScript — health functions
# =====================================================================
print("\n== JS: Health strip logic ==")

js_path = os.path.join(BASE, 'js', 'app.js')
with open(js_path, 'r') as f:
    js = f.read()

check("signalBarsHtml function", 'function signalBarsHtml' in js)
check("drawHealthStrip function", 'function drawHealthStrip' in js)
check("lastFetchTime variable", 'var lastFetchTime' in js)
check("avgLatency variable", 'var avgLatency' in js)

# signalBarsHtml internals
sig_func = js[js.find('function signalBarsHtml'):js.find('function drawHealthStrip')]
check("signalBarsHtml takes rssi param", 'rssi' in sig_func)
check("signalBarsHtml checks -60 threshold", '-60' in sig_func)
check("signalBarsHtml checks -70 threshold", '-70' in sig_func)
check("signalBarsHtml checks -80 threshold", '-80' in sig_func)
check("signalBarsHtml checks -90 threshold", '-90' in sig_func)
check("signalBarsHtml checks -100 threshold", '-100' in sig_func)
check("signalBarsHtml generates 5 bars", 'i <= 5' in sig_func)
check("signalBarsHtml uses signal-bar class", 'signal-bar' in sig_func)
check("signalBarsHtml assigns active/warn/fail", 'active' in sig_func and 'warn' in sig_func and 'fail' in sig_func)

# drawHealthStrip internals
health_start = js.find('function drawHealthStrip')
health_end = js.find('\n// =', health_start)
if health_end < 0:
    health_end = js.find('\nfunction ', health_start + 1)
health_func = js[health_start:health_end]
check("drawHealthStrip reads health-strip", 'health-strip' in health_func)
check("drawHealthStrip shows node card", 'conn-pulse' in health_func)
check("drawHealthStrip shows signal card", 'signalBarsHtml' in health_func)
check("drawHealthStrip shows uptime card", 'formatUptime' in health_func)
check("drawHealthStrip shows latency card", 'avgLatency' in health_func)
check("drawHealthStrip uses health-card class", 'health-card' in health_func)
check("drawHealthStrip uses health-label class", 'health-label' in health_func)
check("drawHealthStrip uses health-val class", 'health-val' in health_func)
check("drawHealthStrip online/offline pulse", 'offline' in health_func)

# Latency tracking in refresh
refresh_func = js[js.find('function refresh'):js.find('\n\n', js.find('function refresh') + 20)]
check("refresh sets lastFetchTime", 'lastFetchTime = Date.now()' in refresh_func)
check("refresh calculates latency", 'Date.now() - lastFetchTime' in refresh_func)
check("refresh computes avgLatency EMA", 'avgLatency * 0.7' in refresh_func or 'avgLatency*0.7' in refresh_func)

# drawHealthStrip called in refresh
check("drawHealthStrip called in refresh", 'drawHealthStrip(s)' in refresh_func)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*40}")
print(f"  {passed} passed, {failed} failed")
print(f"{'='*40}")
sys.exit(1 if failed else 0)
