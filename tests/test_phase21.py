#!/usr/bin/env python3
"""
Phase 21 tests: micro-interactions and animation polish.
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
#  CSS — micro-interaction styles
# =====================================================================
print("\n== CSS: Micro-interaction animations ==")

css_path = os.path.join(BASE, 'css', 'style.css')
with open(css_path, 'r') as f:
    css = f.read()

# Phase 21 section marker
check("Phase 21 comment exists", '/* Phase 21: Micro-interactions */' in css)

# card-enter keyframes
check("@keyframes card-enter", '@keyframes card-enter' in css)
card_enter_kf = css.split('@keyframes card-enter')[1].split('}')[1]
check("card-enter has opacity 0 in from", 'opacity: 0' in css.split('@keyframes card-enter')[1].split('}')[0])
check("card-enter has translateY in from", 'translateY' in css.split('@keyframes card-enter')[1].split('}')[0])

# card-entering class
check(".hero-card.card-entering style", '.hero-card.card-entering' in css)
entering_css = css.split('.hero-card.card-entering')[1].split('}')[0]
check("card-entering uses card-enter animation", 'card-enter' in entering_css)
check("card-entering has both fill mode", 'both' in entering_css)

# val-flash keyframes
check("@keyframes val-flash", '@keyframes val-flash' in css)
flash_kf = css.split('@keyframes val-flash')[1].split('}')[1]
check("val-flash uses accent color", 'var(--accent)' in css.split('@keyframes val-flash')[1].split('}')[0])
check("val-flash has scale", 'scale' in css.split('@keyframes val-flash')[1].split('}')[0])

# value-flash class
check(".hero-value.value-flash style", '.hero-value.value-flash' in css)
vf_css = css.split('.hero-value.value-flash')[1].split('}')[0]
check("value-flash uses val-flash animation", 'val-flash' in vf_css)

# dot-state-pulse keyframes
check("@keyframes dot-state-pulse", '@keyframes dot-state-pulse' in css)
dot_kf = css.split('@keyframes dot-state-pulse')[1]
check("dot-state-pulse has scale 1.6", '1.6' in dot_kf.split('}')[1])

# dot-pulse class
check(".status-dot.dot-pulse style", '.status-dot.dot-pulse' in css)
dp_css = css.split('.status-dot.dot-pulse')[1].split('}')[0]
check("dot-pulse uses dot-state-pulse animation", 'dot-state-pulse' in dp_css)

# health-val-flash class
check(".health-val.health-val-flash style", '.health-val.health-val-flash' in css)
hvf_css = css.split('.health-val.health-val-flash')[1].split('}')[0]
check("health-val-flash uses val-flash animation", 'val-flash' in hvf_css)

# Hero card hover lift
p21_hero = css.split('/* Phase 21: Micro-interactions */')[1]
hero_block = p21_hero.split('.hero-card {')[1].split('}')[0] if '.hero-card {' in p21_hero else ''
check("Phase 21 hero-card transition includes transform", 'transform' in hero_block)
check(".hero-card:hover in Phase 21", '.hero-card:hover' in p21_hero)
hover_css = p21_hero.split('.hero-card:hover')[1].split('}')[0]
check("hero-card hover translateY", 'translateY(-2px)' in hover_css)

# Reduced motion respects Phase 21 animations
reduced_idx = css.rfind('prefers-reduced-motion')
reduced_block = css[reduced_idx:]
check("reduced-motion for card-entering", 'card-entering' in reduced_block)
check("reduced-motion for value-flash", 'value-flash' in reduced_block)
check("reduced-motion for dot-pulse", 'dot-pulse' in reduced_block)
check("reduced-motion for health-val-flash", 'health-val-flash' in reduced_block)
check("reduced-motion disables hover transform", 'hero-card:hover' in reduced_block)

# No duplicate hero-card transition from Phase 12
phase12_section = css.split('/* Phase 12: Smooth card entrance */')[1].split('/* Phase 14')[0] if '/* Phase 12: Smooth card entrance */' in css else ''
check("no duplicate hero-card transition in Phase 12", '.hero-card {' not in phase12_section)

# =====================================================================
#  JavaScript — value change detection and stagger
# =====================================================================
print("\n== JS: Value change detection and stagger ==")

js_path = os.path.join(BASE, 'js', 'app.js')
with open(js_path, 'r') as f:
    js = f.read()

# State variables
check("prevValues variable declared", 'var prevValues' in js)
check("prevValues initialized as object", 'prevValues = {}' in js or 'prevValues = {' in js)
check("heroFirstDraw variable declared", 'var heroFirstDraw' in js)
check("heroFirstDraw initialized true", 'heroFirstDraw = true' in js)
check("prevNodeOnline variable declared", 'var prevNodeOnline' in js)
check("prevNodeOnline initialized null", 'prevNodeOnline = null' in js)

# drawSensorHero stagger logic
hero_func = js[js.find('function drawSensorHero'):js.find('function drawStatusBar')]
check("drawSensorHero tracks newValues", 'newValues' in hero_func)
check("drawSensorHero checks heroFirstDraw", 'heroFirstDraw' in hero_func)
check("drawSensorHero adds card-entering class", "card-entering" in hero_func)
check("drawSensorHero sets animationDelay", 'animationDelay' in hero_func)
check("drawSensorHero stagger delay uses 80ms", '80' in hero_func)
check("drawSensorHero sets heroFirstDraw false", 'heroFirstDraw = false' in hero_func)
check("drawSensorHero adds value-flash class", 'value-flash' in hero_func)
check("drawSensorHero listens animationend", 'animationend' in hero_func)
check("drawSensorHero compares prevValues", 'prevValues[k]' in hero_func or 'prevValues[' in hero_func)
check("drawSensorHero updates prevValues at end", 'prevValues = newValues' in hero_func)

# drawStatusBar dot pulse
status_func_start = js.find('function drawStatusBar')
status_func_end = js.find('\nvar lastFetchTime')
status_func = js[status_func_start:status_func_end]
check("drawStatusBar checks prevNodeOnline", 'prevNodeOnline' in status_func)
check("drawStatusBar adds dot-pulse class", 'dot-pulse' in status_func)
check("drawStatusBar listens animationend for dot", 'animationend' in status_func)
check("drawStatusBar updates prevNodeOnline", 'prevNodeOnline = s.node_online' in status_func)

# drawHealthStrip value flash
health_start = js.find('function drawHealthStrip')
health_end = js.find('\n// =', health_start)
if health_end < 0:
    health_end = js.find('\nfunction ', health_start + 1)
health_func = js[health_start:health_end]
check("drawHealthStrip captures old values", 'oldVals' in health_func or 'oldTexts' in health_func)
check("drawHealthStrip adds health-val-flash", 'health-val-flash' in health_func)
check("drawHealthStrip compares old and new text", 'oldTexts' in health_func)
check("drawHealthStrip listens animationend", 'animationend' in health_func)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*40}")
print(f"  {passed} passed, {failed} failed")
print(f"{'='*40}")
sys.exit(1 if failed else 0)
