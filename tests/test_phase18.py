#!/usr/bin/env python3
"""
Phase 18 tests: skeleton loading states and empty-state illustrations.
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
#  HTML — skeleton placeholders
# =====================================================================
print("\n== HTML: Skeleton placeholders ==")

html_path = os.path.join(BASE, 'index.html')
with open(html_path, 'r') as f:
    html = f.read()

check("skeleton-card in sensor-hero", 'skeleton-card' in html)
hero_section = html[html.find('id="sensor-hero"'):html.find('id="stats-row"')]
check("three skeleton cards", hero_section.count('skeleton-card') == 3)
check("skel-icon in skeleton card", 'skel-icon' in hero_section)
check("skel-value in skeleton card", 'skel-value' in hero_section)
check("skel-label in skeleton card", 'skel-label' in hero_section)
check("skel-status-line in status bar", 'skel-status-line' in html)
status_bar = html[html.find('id="status-bar"'):html.find('id="status-bar"') + 200]
check("status bar has skeleton line", 'skel-status-line' in status_bar)
check("skeletons before chart section", html.find('skeleton-card') < html.find('chart-section'))

# =====================================================================
#  CSS — skeleton styles
# =====================================================================
print("\n== CSS: Skeleton and empty-state styles ==")

css_path = os.path.join(BASE, 'css', 'style.css')
with open(css_path, 'r') as f:
    css = f.read()

check(".skeleton-card style", '.skeleton-card' in css)
check(".skel base style", '\n.skel ' in css or '\n.skel{' in css)
skel_css = css[css.index('.skel ') if '.skel ' in css else css.index('.skel{'):]
skel_block = skel_css.split('}')[0]
check("skel uses border-radius", 'border-radius' in skel_block)
check("skel uses animation", 'animation' in skel_block)
check("skel background from theme var", 'var(--border)' in skel_block)

check("skel-shimmer keyframes", '@keyframes skel-shimmer' in css)
shimmer = css.split('@keyframes skel-shimmer')[1].split('@keyframes')[0] if '@keyframes skel-shimmer' in css else ''
check("shimmer animates opacity", 'opacity' in shimmer)

check(".skel-icon style", '.skel-icon' in css)
icon_skel = css.split('.skel-icon')[1].split('}')[0]
check("skel-icon has width", 'width' in icon_skel)
check("skel-icon has height", 'height' in icon_skel)
check("skel-icon is round", 'border-radius' in icon_skel and '50%' in icon_skel)

check(".skel-value style", '.skel-value' in css)
val_skel = css.split('.skel-value')[1].split('}')[0]
check("skel-value has width", 'width' in val_skel)
check("skel-value has height", 'height' in val_skel)

check(".skel-label style", '.skel-label' in css)
check(".skel-status-line style", '.skel-status-line' in css)
line_skel = css.split('.skel-status-line')[1].split('}')[0]
check("skel-status-line full width", '100%' in line_skel)

check(".skeleton-card pointer-events none", 'pointer-events' in css.split('.skeleton-card')[1].split('}')[0])

# Empty-state styles
check(".empty-state-box style", '.empty-state-box' in css)
es_css = css.split('.empty-state-box')[1].split('}')[0]
check("empty-state-box centered", 'center' in es_css)
check("empty-state-box has padding", 'padding' in es_css)

check(".empty-state-icon style", '.empty-state-icon' in css)
check(".empty-state-icon svg sizing", '.empty-state-icon svg' in css)
check(".empty-state-title style", '.empty-state-title' in css)
check(".empty-state-msg style", '.empty-state-msg' in css)
check(".empty-state-msg a style", '.empty-state-msg a' in css)

# =====================================================================
#  JavaScript — skeleton lifecycle
# =====================================================================
print("\n== JS: Skeleton lifecycle ==")

js_path = os.path.join(BASE, 'js', 'app.js')
with open(js_path, 'r') as f:
    js = f.read()

# drawSensorHero replaces skeleton content (sets innerHTML)
hero_func = js[js.find('function drawSensorHero'):js.find('function drawStatusBar')]
check("drawSensorHero sets innerHTML (clears skeletons)", 'box.innerHTML' in hero_func)

# Empty-state illustration in hero
check("empty-state-box in hero empty state", 'empty-state-box' in hero_func)
check("empty-state-icon SVG in hero", 'empty-state-icon' in hero_func)
check("empty-state-title in hero", 'empty-state-title' in hero_func)
check("empty-state-msg in hero", 'empty-state-msg' in hero_func)
check("Hardware Studio link in empty state", 'Hardware Studio' in hero_func)

# Chart empty state
chart_func = js[js.find('function drawChart'):js.find('\n\n', js.find('function drawChart') + 20)]
check("chart-empty class in drawChart", 'chart-empty' in chart_func)
check("chart empty-state-box", 'empty-state-box' in chart_func)
check("chart empty-state-icon SVG", 'empty-state-icon' in chart_func)
check("chart empty removes when data arrives", 'removeChild' in chart_func)
check("chart canvas hidden when empty", "canvas.style.display = 'none'" in chart_func or 'canvas.style.display="none"' in chart_func)
check("chart canvas shown when data", "canvas.style.display = ''" in chart_func or 'canvas.style.display=""' in chart_func)

# drawStatusBar replaces skeleton (sets innerHTML)
status_start = js.find('function drawStatusBar')
status_end = js.find('\nfunction ', status_start + 1)
status_func = js[status_start:status_end]
check("drawStatusBar sets innerHTML (clears skeleton)", 'bar.innerHTML' in status_func)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*40}")
print(f"  {passed} passed, {failed} failed")
print(f"{'='*40}")
sys.exit(1 if failed else 0)
