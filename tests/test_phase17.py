#!/usr/bin/env python3
"""
Phase 17 tests: tap-to-expand sensor detail cards.
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
print("\n== HTML: Detail overlay container ==")

html_path = os.path.join(BASE, 'index.html')
with open(html_path, 'r') as f:
    html = f.read()

check("detail-overlay exists", 'id="detail-overlay"' in html)
check("detail-overlay has class", 'class="detail-overlay"' in html)
check("detail-card exists", 'id="detail-card"' in html)
check("detail-card has class", 'class="detail-card"' in html)
check("detail-card inside detail-overlay", html.find('detail-overlay') < html.find('detail-card'))
check("detail-overlay before dashboard view", html.find('detail-overlay') < html.find('id="view-dashboard"'))

# =====================================================================
#  CSS — detail overlay styles
# =====================================================================
print("\n== CSS: Detail overlay styles ==")

css_path = os.path.join(BASE, 'css', 'style.css')
with open(css_path, 'r') as f:
    css = f.read()

check(".detail-overlay style", '\n.detail-overlay {' in css or '\n.detail-overlay{' in css)
overlay_start = css.index('\n.detail-overlay {') + 1 if '\n.detail-overlay {' in css else css.index('\n.detail-overlay{') + 1
overlay_css = css[overlay_start:].split('}')[0]
check("overlay position fixed", 'fixed' in overlay_css)
check("overlay z-index", 'z-index' in overlay_css)
check("overlay flex layout", 'flex' in overlay_css)
check("overlay opacity 0 initial", 'opacity' in overlay_css and '0' in overlay_css)
check("overlay pointer-events none", 'pointer-events' in overlay_css and 'none' in overlay_css)
check("overlay transition", 'transition' in overlay_css)

check(".detail-overlay.active style", '.detail-overlay.active' in css)
active_css = css.split('.detail-overlay.active')[1].split('}')[0]
check("active opacity 1", 'opacity' in active_css and '1' in active_css)
check("active pointer-events auto", 'pointer-events' in active_css and 'auto' in active_css)

check(".detail-card style", '\n.detail-card {' in css or '\n.detail-card{' in css)
card_idx = css.index('\n.detail-card {') + 1 if '\n.detail-card {' in css else css.index('\n.detail-card{') + 1
card_block = css[card_idx:].split('}')[0]
check("card background", 'var(--card)' in card_block)
check("card border", 'var(--border)' in card_block)
check("card border-radius", 'border-radius' in card_block)
check("card max-width", 'max-width' in card_block)
check("card transform translateY", 'translateY' in card_block)
check("card transition", 'transition' in card_block)
check("card max-height", 'max-height' in card_block)
check("card overflow-y auto", 'overflow-y' in card_block)
check("card safe-area padding", 'safe-area-inset-bottom' in card_block)

check(".detail-header style", '.detail-header' in css)
check(".detail-icon style", '.detail-icon' in css)
check(".detail-title style", '.detail-title' in css)
check(".detail-sub style", '.detail-sub' in css)
check(".detail-value-row style", '.detail-value-row' in css)
check(".detail-big-value style", '.detail-big-value' in css)
check(".detail-big-unit style", '.detail-big-unit' in css)
check(".detail-trend style", '.detail-trend' in css)
check(".detail-spark style", '.detail-spark' in css)
check(".detail-stats style", '.detail-stats' in css)
check(".detail-stat style", '.detail-stat' in css)
check(".detail-stat-label style", '.detail-stat-label' in css)
check(".detail-stat-value style", '.detail-stat-value' in css)
check(".detail-meta style", '.detail-meta' in css)
check(".detail-meta-item style", '.detail-meta-item' in css)
check(".detail-thresh-bar style", '.detail-thresh-bar' in css)
check(".detail-thresh-fill style", '.detail-thresh-fill' in css)
check(".detail-thresh-label style", '.detail-thresh-label' in css)

# Hero card tap affordance
check("hero-card cursor pointer", 'cursor' in css.split('.hero-card')[1].split('}')[0] and 'pointer' in css.split('.hero-card')[1].split('}')[0])
check("hero-card active scale", '.hero-card:active' in css)

# Stats grid layout
stats_css = css.split('.detail-stats')[1].split('}')[0]
check("stats uses grid or flex", 'grid' in stats_css or 'flex' in stats_css)

# Reduced motion
reduced_block = css[css.rfind('prefers-reduced-motion'):]
check("reduced-motion for detail overlay", 'detail-overlay' in reduced_block or 'detail-card' in reduced_block)

# =====================================================================
#  JavaScript — detail overlay functions
# =====================================================================
print("\n== JS: Detail overlay logic ==")

js_path = os.path.join(BASE, 'js', 'app.js')
with open(js_path, 'r') as f:
    js = f.read()

check("lastState variable declared", 'var lastState' in js)
check("lastState assigned in refresh", 'lastState = s' in js)

check("openSensorDetail function", 'function openSensorDetail' in js)
check("closeSensorDetail function", 'function closeSensorDetail' in js)
check("detailSparklineSvg function", 'function detailSparklineSvg' in js)
check("getThresholdProximity function", 'function getThresholdProximity' in js)
check("detailOverlay variable", 'var detailOverlay' in js)
check("detailCard variable", 'var detailCard' in js)

# openSensorDetail internals
open_func = js[js.find('function openSensorDetail'):js.find('function closeSensorDetail')]
check("openSensorDetail reads lastState sensors", 'lastState' in open_func and 'sensors' in open_func)
check("openSensorDetail uses sensorMeta", 'sensorMeta' in open_func)
check("openSensorDetail calls computeTrend", 'computeTrend' in open_func)
check("openSensorDetail calls computeStats", 'computeStats' in open_func)
check("openSensorDetail calls getSensorIcon", 'getSensorIcon' in open_func)
check("openSensorDetail calls detailSparklineSvg", 'detailSparklineSvg' in open_func)
check("openSensorDetail calls getThresholdProximity", 'getThresholdProximity' in open_func)
check("openSensorDetail calls formatAge", 'formatAge' in open_func)
check("openSensorDetail calls escHtml", 'escHtml' in open_func)
check("openSensorDetail adds active class", "classList.add('active')" in open_func)
check("openSensorDetail locks body scroll", "body.style.overflow" in open_func)
check("openSensorDetail builds detail-header", 'detail-header' in open_func)
check("openSensorDetail builds detail-value-row", 'detail-value-row' in open_func)
check("openSensorDetail builds detail-stats", 'detail-stats' in open_func)
check("openSensorDetail builds detail-meta", 'detail-meta' in open_func)

# closeSensorDetail internals
close_func = js[js.find('function closeSensorDetail'):js.find('\n\n', js.find('function closeSensorDetail'))]
check("closeSensorDetail removes active class", "classList.remove('active')" in close_func)
check("closeSensorDetail restores body scroll", "body.style.overflow" in close_func)

# detailSparklineSvg internals
spark_func = js[js.find('function detailSparklineSvg'):js.find('function getThresholdProximity')]
check("detailSparklineSvg takes w and h params", 'key, w, h' in spark_func)
check("detailSparklineSvg uses chartData", 'chartData' in spark_func)
check("detailSparklineSvg draws polyline", 'polyline' in spark_func)
check("detailSparklineSvg uses more data points", 'slice(-60)' in spark_func)

# getThresholdProximity internals
thresh_func = js[js.find('function getThresholdProximity'):js.find('function openSensorDetail')]
check("getThresholdProximity reads configData rules", 'configData' in thresh_func and 'rules' in thresh_func)
check("getThresholdProximity returns percent", 'percent' in thresh_func)
check("getThresholdProximity returns threshold", 'threshold' in thresh_func)

# Event delegation on hero cards
check("hero-card data-sensor-key attribute in drawSensorHero", 'data-sensor-key' in js)
hero_func = js[js.find('function drawSensorHero'):js.find('function drawStatusBar')]
check("drawSensorHero adds data-sensor-key to cards", 'data-sensor-key' in hero_func)

# Click handler uses event delegation on sensor-hero
init_block = js[js.find('initDetailOverlay'):]
check("event delegation on sensor-hero", 'sensor-hero' in init_block)
check("closest hero-card lookup", "closest('.hero-card" in init_block or 'closest(".hero-card' in init_block)
check("getAttribute data-sensor-key", "getAttribute('data-sensor-key')" in init_block or 'getAttribute("data-sensor-key")' in init_block)

# Backdrop click to close
check("overlay click closes detail", 'e.target === detailOverlay' in init_block or 'e.target===detailOverlay' in init_block)

# Escape key to close
check("Escape key closes detail", "'Escape'" in init_block or '"Escape"' in init_block)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*40}")
print(f"  {passed} passed, {failed} failed")
print(f"{'='*40}")
sys.exit(1 if failed else 0)
