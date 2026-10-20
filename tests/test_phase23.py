#!/usr/bin/env python3
"""
Phase 23 tests: richer environment differentiation.
Covers expanded PROFILES (description, idealRanges, tips, more presets),
HTML env-context section, CSS range/tips styles, and JS helper functions.
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
#  JS — Expanded PROFILES
# =====================================================================
print("\n== JS: Expanded PROFILES ==")

js_path = os.path.join(BASE, 'js', 'app.js')
with open(js_path, 'r') as f:
    js = f.read()

ENV_TYPES = ['home', 'farm', 'greenhouse', 'industrial', 'lab', 'custom']

for env in ENV_TYPES:
    # Find the profile block
    marker = env + ': {'
    check(f"{env} profile exists", marker in js)

# description field
for env in ENV_TYPES:
    block_start = js.index(env + ': {')
    block = js[block_start:block_start + 300]
    check(f"{env} has description", 'description:' in block)

# idealRanges field
for env in ENV_TYPES:
    block_start = js.index(env + ': {')
    block = js[block_start:block_start + 400]
    check(f"{env} has idealRanges", 'idealRanges:' in block)

# tips field
for env in ENV_TYPES:
    block_start = js.index(env + ': {')
    block = js[block_start:block_start + 800]
    check(f"{env} has tips", 'tips:' in block)

# defaultRelayCount field
for env in ENV_TYPES:
    block_start = js.index(env + ': {')
    block = js[block_start:block_start + 300]
    check(f"{env} has defaultRelayCount", 'defaultRelayCount:' in block)

# home idealRanges specifics
home_start = js.index("home: {")
home_block = js[home_start:home_start + 500]
check("home temp range 18-26", 'temperature_c: [18, 26]' in home_block)
check("home humidity range 30-60", 'humidity_pct: [30, 60]' in home_block)

# lab idealRanges specifics (tight)
lab_start = js.index("lab: {", js.index("industrial:"))
lab_block = js[lab_start:lab_start + 500]
check("lab temp range 20-25", 'temperature_c: [20, 25]' in lab_block)
check("lab humidity range 30-55", 'humidity_pct: [30, 55]' in lab_block)

# industrial defaultRelayCount = 8
ind_start = js.index("industrial: {")
ind_block = js[ind_start:ind_start + 300]
check("industrial defaultRelayCount is 8", 'defaultRelayCount: 8' in ind_block)

# Expanded presets — each env should have >= 4 presets (except custom)
for env in ['home', 'farm', 'greenhouse', 'industrial', 'lab']:
    block_start = js.index(env + ': {')
    # Find presets array
    presets_start = js.index('presets: [', block_start)
    presets_end = js.index(']', presets_start)
    presets_block = js[presets_start:presets_end]
    preset_count = presets_block.count('{ name:')
    check(f"{env} has >= 4 presets (has {preset_count})", preset_count >= 4)

# Specific new presets
check("home has 'Winter heating' preset", "'Winter heating'" in js)
check("farm has 'Frost alert' preset", "'Frost alert'" in js)
check("greenhouse has 'Humidity misting' preset", "'Humidity misting'" in js)
check("industrial has 'Humidity guard' preset", "'Humidity guard'" in js)
check("lab has 'Fume extraction' preset", "'Fume extraction'" in js)
check("lab has 'Cold storage' preset", "'Cold storage'" in js)

# =====================================================================
#  JS — getRangeBadge function
# =====================================================================
print("\n== JS: getRangeBadge function ==")

check("getRangeBadge function exists", 'function getRangeBadge(' in js)
fn_start = js.index('function getRangeBadge(')
fn_block = js[fn_start:fn_start + 500]
check("getRangeBadge checks idealRanges", 'idealRanges' in fn_block)
check("getRangeBadge returns in-range class", 'in-range' in fn_block)
check("getRangeBadge returns out-of-range class", 'out-of-range' in fn_block)
check("getRangeBadge shows Below ideal", "'Below'" in fn_block or '"Below"' in fn_block)
check("getRangeBadge shows Above ideal", "'Above'" in fn_block or '"Above"' in fn_block)

# =====================================================================
#  JS — drawEnvContext function
# =====================================================================
print("\n== JS: drawEnvContext function ==")

check("drawEnvContext function exists", 'function drawEnvContext()' in js)
fn_start = js.index('function drawEnvContext()')
fn_block = js[fn_start:fn_start + 800]
check("drawEnvContext accesses env-context", "'env-context'" in fn_block)
check("drawEnvContext accesses env-desc", "'env-desc'" in fn_block)
check("drawEnvContext accesses env-tips-list", "'env-tips-list'" in fn_block)
check("drawEnvContext renders tips", 'currentProfile.tips' in fn_block)
check("drawEnvContext renders description", 'currentProfile.description' in fn_block)

# drawEnvContext is called when profile changes
check("drawEnvContext called on profile change", 'drawEnvContext()' in js)

# rangeBadge injected in hero card
hero_card_area = js[js.index('function drawSensorHero'):js.index('function drawSensorHero') + 3000]
check("getRangeBadge called in drawSensorHero", 'getRangeBadge(' in hero_card_area)
check("rangeBadge inserted in hero card HTML", 'rangeBadge' in hero_card_area)

# Wizard shows description
wiz_hint_area = js[js.index('function updateProfileHint'):js.index('function updateProfileHint') + 500]
check("Wizard hint shows profile description", 'prof.description' in wiz_hint_area)

# =====================================================================
#  HTML — Environment context section
# =====================================================================
print("\n== HTML: Environment context section ==")

html_path = os.path.join(BASE, 'index.html')
with open(html_path, 'r') as f:
    html = f.read()

check("env-context container exists", 'id="env-context"' in html)
check("env-desc element exists", 'id="env-desc"' in html)
check("env-tips details element exists", 'id="env-tips"' in html)
check("env-tips-list ul exists", 'id="env-tips-list"' in html)
check("Tips summary text", 'Tips for your environment' in html)
check("env-context initially hidden", 'id="env-context" style="display:none"' in html)

# Verify placement: env-context is between sensor-hero and stats-row
hero_pos = html.index('id="sensor-hero"')
context_pos = html.index('id="env-context"')
stats_pos = html.index('id="stats-row"')
check("env-context between hero and stats", hero_pos < context_pos < stats_pos)

# =====================================================================
#  CSS — Phase 23 styles
# =====================================================================
print("\n== CSS: Phase 23 styles ==")

css_path = os.path.join(BASE, 'css', 'style.css')
with open(css_path, 'r') as f:
    css = f.read()

check("Phase 23 comment exists", '/* Phase 23: Environment context and range indicators */' in css)

# env-context
check(".env-context class", '.env-context' in css)

# env-desc
check(".env-desc class", '.env-desc' in css)
desc_css = css[css.index('.env-desc'):css.index('.env-desc') + 200]
check("env-desc uses muted color", 'var(--muted)' in desc_css)
check("env-desc is centered", 'text-align: center' in desc_css)

# env-tips
check(".env-tips class", '.env-tips' in css)
check(".env-tips summary class", '.env-tips summary' in css)
tips_summary = css[css.index('.env-tips summary'):css.index('.env-tips summary') + 200]
check("tips summary uses accent color", 'var(--accent)' in tips_summary)

check(".env-tips ul class", '.env-tips ul' in css)
check(".env-tips li class", '.env-tips li' in css)

# hero-range badge
check(".hero-range class", '.hero-range' in css)
check(".hero-range.in-range class", '.hero-range.in-range' in css)
in_range_css = css[css.index('.hero-range.in-range'):css.index('.hero-range.in-range') + 150]
check("in-range uses --on color", 'var(--on)' in in_range_css)

check(".hero-range.out-of-range class", '.hero-range.out-of-range' in css)
out_range_css = css[css.index('.hero-range.out-of-range'):css.index('.hero-range.out-of-range') + 150]
check("out-of-range uses --fail color", 'var(--fail)' in out_range_css)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 23 tests: {passed} passed, {failed} failed out of {passed + failed}")
print(f"{'='*60}")

if failed > 0:
    sys.exit(1)
