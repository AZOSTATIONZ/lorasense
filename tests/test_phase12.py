#!/usr/bin/env python3
"""
Phase 12 tests: app polish — PWA manifest, view transitions, pull-to-refresh,
connection resilience, and toggle feedback.
"""
import sys
import json
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

BASE = os.path.join(os.path.dirname(__file__), '..', 'firmware', 'gateway-esp32', 'data')

# =====================================================================
#  PWA manifest
# =====================================================================
print("\n== PWA manifest ==")

manifest_path = os.path.join(BASE, 'manifest.json')
check("manifest.json exists", os.path.isfile(manifest_path))

with open(manifest_path, 'r') as f:
    manifest = json.load(f)

check("manifest has name", manifest.get('name') == 'LoRaSense')
check("manifest has short_name", manifest.get('short_name') == 'LoRaSense')
check("manifest display is standalone", manifest.get('display') == 'standalone')
check("manifest has start_url", manifest.get('start_url') == '/')
check("manifest has theme_color", 'theme_color' in manifest)
check("manifest has background_color", 'background_color' in manifest)
check("manifest has icons array", isinstance(manifest.get('icons'), list))
check("manifest has at least one icon", len(manifest.get('icons', [])) >= 1)

# =====================================================================
#  HTML meta tags
# =====================================================================
print("\n== HTML meta tags ==")

html_path = os.path.join(BASE, 'index.html')
with open(html_path, 'r') as f:
    html = f.read()

check("manifest link tag", 'rel="manifest"' in html)
check("apple-mobile-web-app-capable", 'apple-mobile-web-app-capable' in html)
check("theme-color meta", 'name="theme-color"' in html)
check("viewport-fit=cover", 'viewport-fit=cover' in html)

# =====================================================================
#  Reconnect overlay HTML
# =====================================================================
print("\n== Reconnect overlay ==")

check("reconnect-overlay div exists", 'id="reconnect-overlay"' in html)
check("reconnect-countdown span", 'id="reconnect-countdown"' in html)
check("reconnect-now-btn", 'id="reconnect-now-btn"' in html)
check("reconnect-progress bar", 'id="reconnect-progress"' in html)
check("reconnect-card class", 'reconnect-card' in html)

# =====================================================================
#  Pull-to-refresh HTML
# =====================================================================
print("\n== Pull-to-refresh ==")

check("pull-indicator div", 'id="pull-indicator"' in html)
check("pull-spinner SVG", 'pull-spinner' in html)

# =====================================================================
#  CSS features
# =====================================================================
print("\n== CSS features ==")

css_path = os.path.join(BASE, 'css', 'style.css')
with open(css_path, 'r') as f:
    css = f.read()

check("view-enter class", '.view-enter' in css)
check("view-active class", '.view-active' in css)
check("pull-indicator styles", '.pull-indicator' in css)
check("pull-indicator.pulling", '.pull-indicator.pulling' in css)
check("pull-indicator.refreshing", '.pull-indicator.refreshing' in css)
check("spin keyframes", '@keyframes spin' in css)
check("reconnect-overlay styles", '.reconnect-overlay' in css)
check("reconnect-overlay.active", '.reconnect-overlay.active' in css)
check("reconnect-card styles", '.reconnect-card' in css)
check("reconnect-in keyframes", '@keyframes reconnect-in' in css)
check("reconnect-progress bar", '.reconnect-progress' in css)
check("toggle active feedback", '.toggle:active' in css)
check("btn-small active feedback", '.btn-small:active' in css)
check("hero-card transition", 'transition' in css and '.hero-card' in css)
check("device active feedback", '.device:active' in css)

# =====================================================================
#  JavaScript features
# =====================================================================
print("\n== JavaScript features ==")

js_path = os.path.join(BASE, 'js', 'app.js')
with open(js_path, 'r') as f:
    js = f.read()

check("failCount variable", 'var failCount' in js)
check("reconnectTimer variable", 'var reconnectTimer' in js)
check("RECONNECT_DELAYS array", 'RECONNECT_DELAYS' in js)
check("showReconnectOverlay function", 'function showReconnectOverlay' in js)
check("hideReconnectOverlay function", 'function hideReconnectOverlay' in js)
check("startReconnectCountdown function", 'function startReconnectCountdown' in js)
check("reconnect-now-btn handler", "reconnect-now-btn" in js)
check("failCount increments on error", 'failCount++' in js)
check("failCount resets on success", 'failCount = 0' in js)
check("shows overlay after 3 failures", 'failCount >= 3' in js)

check("view-enter class added in showView", 'view-enter' in js)
check("view-active class added in showView", 'view-active' in js)

check("pull-to-refresh touchstart listener", 'touchstart' in js)
check("pull-to-refresh touchmove listener", 'touchmove' in js)
check("pull-to-refresh touchend listener", 'touchend' in js)
check("pull uses passive listeners", 'passive: true' in js)
check("pull checks dashboard view", "currentView !== 'dashboard'" in js)
check("pull refreshing class", "'refreshing'" in js)

# =====================================================================
#  Backoff logic validation
# =====================================================================
print("\n== Reconnect backoff ==")

delays_match = re.search(r'RECONNECT_DELAYS\s*=\s*\[([^\]]+)\]', js)
check("delays array found", delays_match is not None)
if delays_match:
    delays = [int(x.strip()) for x in delays_match.group(1).split(',')]
    check("delays are increasing", all(delays[i] <= delays[i+1] for i in range(len(delays)-1)))
    check("first delay is short (<=5s)", delays[0] <= 5)
    check("max delay is reasonable (<=60s)", delays[-1] <= 60)
    check("at least 3 delay steps", len(delays) >= 3)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*40}")
print(f"  {passed} passed, {failed} failed")
print(f"{'='*40}")
sys.exit(1 if failed else 0)
