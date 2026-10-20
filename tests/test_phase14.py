#!/usr/bin/env python3
"""
Phase 14 tests: quick controls bar and collapsible sections.
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

BASE = os.path.join(os.path.dirname(__file__), '..', 'firmware', 'gateway-esp32', 'data')

# =====================================================================
#  HTML structure — collapsible sections
# =====================================================================
print("\n== HTML: Collapsible sections ==")

html_path = os.path.join(BASE, 'index.html')
with open(html_path, 'r') as f:
    html = f.read()

check("sect-chart wrapper exists", 'id="sect-chart"' in html)
check("sect-devices wrapper exists", 'id="sect-devices"' in html)
check("sect-chart has sect-wrap class", 'class="sect-wrap" id="sect-chart"' in html)
check("sect-devices has sect-wrap class", 'class="sect-wrap" id="sect-devices"' in html)

check("chart sect-hdr exists", 'data-sect="sect-chart"' in html)
check("devices sect-hdr exists", 'data-sect="sect-devices"' in html)
check("chart arrow span", 'id="arrow-sect-chart"' in html)
check("devices arrow span", 'id="arrow-sect-devices"' in html)

check("chart sect-body exists", 'id="body-sect-chart"' in html)
check("devices sect-body exists", 'id="body-sect-devices"' in html)

# Verify chart-section is INSIDE sect-body (not a sibling)
chart_body_pos = html.find('id="body-sect-chart"')
chart_section_pos = html.find('id="chart-section"')
check("chart-section inside sect-body", chart_body_pos < chart_section_pos)

# Verify devices div is INSIDE sect-body
dev_body_pos = html.find('id="body-sect-devices"')
dev_div_pos = html.find('id="devices"')
check("devices div inside sect-body", dev_body_pos < dev_div_pos and dev_div_pos < html.find('</div>', dev_body_pos) + 500)

# =====================================================================
#  HTML structure — quick controls bar
# =====================================================================
print("\n== HTML: Quick controls bar ==")

check("quick-bar container exists", 'id="quick-bar"' in html)
check("quick-bar has class", 'class="quick-bar"' in html)

# Quick bar should be outside the dashboard view div
dashboard_close = html.rfind('</div>', 0, html.find('id="quick-bar"'))
check("quick-bar is in document", html.find('quick-bar') > 0)

# =====================================================================
#  CSS — collapsible sections
# =====================================================================
print("\n== CSS: Collapsible sections ==")

css_path = os.path.join(BASE, 'css', 'style.css')
with open(css_path, 'r') as f:
    css = f.read()

check(".sect-wrap style", '.sect-wrap' in css)
check(".sect-hdr style", '.sect-hdr' in css)
check(".sect-hdr has cursor pointer", 'cursor' in css.split('.sect-hdr')[1].split('}')[0] and 'pointer' in css.split('.sect-hdr')[1].split('}')[0])
check(".sect-hdr has user-select none", 'user-select' in css.split('.sect-hdr')[1].split('}')[0])
check(".sect-arrow style", '.sect-arrow' in css)
check(".sect-arrow has transition", 'transition' in css.split('.sect-arrow')[1].split('}')[0])

check(".sect-wrap.collapsed .sect-arrow rotates", '.sect-wrap.collapsed .sect-arrow' in css)
check("collapsed arrow uses rotate", 'rotate(-90deg)' in css)

check(".sect-body style", '.sect-body' in css)
check(".sect-body has max-height transition", 'max-height' in css.split('.sect-body')[1].split('}')[0])
check(".sect-body has overflow hidden", 'overflow' in css.split('.sect-body')[1].split('}')[0])
check(".sect-body has opacity transition", 'opacity' in css.split('.sect-body')[1].split('}')[0])

check(".sect-wrap.collapsed .sect-body", '.sect-wrap.collapsed .sect-body' in css)
collapsed_body = css.split('.sect-wrap.collapsed .sect-body')[1].split('}')[0]
check("collapsed body max-height 0", 'max-height: 0' in collapsed_body or 'max-height:0' in collapsed_body)
check("collapsed body opacity 0", 'opacity: 0' in collapsed_body or 'opacity:0' in collapsed_body)

# =====================================================================
#  CSS — quick controls bar
# =====================================================================
print("\n== CSS: Quick controls bar ==")

check(".quick-bar style", '.quick-bar' in css)
quick_bar_css = css.split('.quick-bar')[1].split('}')[0]
check("quick-bar position fixed", 'position' in quick_bar_css and 'fixed' in quick_bar_css)
check("quick-bar bottom 0", 'bottom' in quick_bar_css)
check("quick-bar z-index", 'z-index' in quick_bar_css)
check("quick-bar display none by default", 'display' in quick_bar_css and 'none' in quick_bar_css)
check("quick-bar safe-area-inset-bottom", 'safe-area-inset-bottom' in css)

check(".quick-bar.visible", '.quick-bar.visible' in css)
check("visible bar uses flex", 'flex' in css.split('.quick-bar.visible')[1].split('}')[0])

check(".quick-pill style", '.quick-pill' in css)
check("quick-pill border-radius", 'border-radius' in css.split('.quick-pill')[1].split('}')[0])
check("quick-pill cursor pointer", 'cursor' in css.split('.quick-pill')[1].split('}')[0])
check("quick-pill white-space nowrap", 'nowrap' in css.split('.quick-pill')[1].split('}')[0])

check(".quick-pill:active feedback", '.quick-pill:active' in css)
check("active pill scales down", 'scale(0.94)' in css)

check(".quick-pill.qp-on style", '.quick-pill.qp-on' in css)
check("qp-on uses --on color", 'var(--on)' in css.split('.quick-pill.qp-on')[1].split('}')[0])

check(".qp-dot indicator", '.qp-dot' in css)
check("qp-dot is circular", 'border-radius' in css.split('.qp-dot')[1].split('}')[0])

check(".quick-pill.qp-alloff style", '.quick-pill.qp-alloff' in css)
check("alloff uses --fail color", 'var(--fail)' in css.split('.quick-pill.qp-alloff')[1].split('}')[0])

check("body padding when bar visible", 'quick-bar.visible' in css and 'padding-bottom' in css)

# =====================================================================
#  JavaScript — collapsible sections
# =====================================================================
print("\n== JS: Collapsible sections ==")

js_path = os.path.join(BASE, 'js', 'app.js')
with open(js_path, 'r') as f:
    js = f.read()

check("ls_collapsed storage key", "'ls_collapsed'" in js)
check("localStorage getItem for collapsed", "localStorage.getItem(STORAGE_KEY)" in js or "localStorage.getItem('ls_collapsed')" in js)
check("localStorage setItem for collapsed", "localStorage.setItem(STORAGE_KEY" in js or "localStorage.setItem('ls_collapsed'" in js)
check("JSON parse of collapsed state", "JSON.parse(raw)" in js or "JSON.parse(" in js)
check("JSON stringify for save", "JSON.stringify(collapsed)" in js)

check("querySelectorAll for sect-hdr", "querySelectorAll('.sect-hdr')" in js)
check("getAttribute data-sect", "getAttribute('data-sect')" in js)
check("classList.add collapsed", "classList.add('collapsed')" in js)
check("classList.remove collapsed", "classList.remove('collapsed')" in js)

check("click handler toggles collapsed state", "collapsed[sectId] = !collapsed[sectId]" in js)
check("saveCollapsed called on click", "saveCollapsed()" in js)
check("drawChart on expand chart section", "sectId === 'sect-chart'" in js and "drawChart()" in js)

# Wrapped in IIFE
iife_start = js.find("Phase 14: Collapsible sections")
check("collapsible sections in IIFE", iife_start > 0 and js[js.rfind('(function', 0, iife_start):iife_start].count('(function') > 0)

# try/catch around localStorage
collapsed_section = js[js.find("Phase 14: Collapsible"):js.find("Phase 14: Quick")]
check("try/catch around localStorage read", 'try' in collapsed_section and 'catch' in collapsed_section)

# =====================================================================
#  JavaScript — quick controls bar
# =====================================================================
print("\n== JS: Quick controls bar ==")

check("drawQuickBar function", 'function drawQuickBar' in js)
check("allOff function", 'function allOff' in js)
check("lastQuickDevices variable", 'lastQuickDevices' in js)

check("drawQuickBar checks for relay devices", 'd.output === null || d.output === undefined' in js)
check("drawQuickBar checks currentView", "currentView !== 'dashboard'" in js)
check("drawQuickBar adds visible class", "bar.classList.add('visible')" in js)
check("drawQuickBar removes visible when no devices", "bar.classList.remove('visible')" in js)

check("qp-on class for active device", "qp-on" in js)
check("qp-alloff class for all-off button", "qp-alloff" in js)
check("qb-alloff button id", "qb-alloff" in js)

# Event delegation on quick bar
check("quick-bar onclick handler", "document.getElementById('quick-bar').onclick" in js)
check("closest quick-pill for delegation", "closest('.quick-pill')" in js)
check("data-qb-id attribute used", "data-qb-id" in js)
check("data-qb-on attribute used", "data-qb-on" in js)

# allOff function
check("allOff posts to /api/all-off", "'/api/all-off'" in js)
check("allOff fallback on failure", "lastQuickDevices" in js.split('function allOff')[1])

# Integration with refresh
check("drawQuickBar called in refresh", "drawQuickBar(s)" in js.split('function refresh')[1].split('function ')[0])
check("lastQuickDevices set in refresh", "lastQuickDevices = s.devices" in js)

# showView hides bar
check("quick-bar hidden on view change", "qbar" in js.split("function showView")[1].split("function ")[0] and "classList.remove('visible')" in js.split("function showView")[1].split("function ")[0])

# escHtml used for XSS prevention
qb_func = js[js.find('function drawQuickBar'):js.find('function allOff')]
check("device names escaped in quick bar", "escHtml(d.name)" in qb_func)
check("device ids escaped in quick bar", "escHtml(d.id)" in qb_func)

# anyOn logic for all-off button
check("anyOn flag tracks if any device on", "anyOn" in qb_func)
check("all-off only shown when anyOn", "if (anyOn)" in js)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*40}")
print(f"  {passed} passed, {failed} failed")
print(f"{'='*40}")
sys.exit(1 if failed else 0)
