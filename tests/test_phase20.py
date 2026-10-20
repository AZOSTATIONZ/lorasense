#!/usr/bin/env python3
"""
Phase 20 tests: accessibility and keyboard navigation.
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
#  HTML — ARIA landmarks and attributes
# =====================================================================
print("\n== HTML: ARIA landmarks ==")

html_path = os.path.join(BASE, 'index.html')
with open(html_path, 'r') as f:
    html = f.read()

check("skip-link exists", 'class="skip-link"' in html)
check("skip-link href to dashboard", 'href="#view-dashboard"' in html)
check("skip-link text", 'Skip to content' in html)
check("skip-link before header", html.find('skip-link') < html.find('<header'))

check("header has role=banner", 'role="banner"' in html)
check("main has role=main", 'role="main"' in html)

check("dark-mode-btn has aria-label", html.find('aria-label="Toggle dark mode"') > 0)
check("settings-btn has aria-label", html.find('aria-label="Settings"') > 0)

# Find the line containing each id to check attributes on that same tag
def tag_with_id(html, element_id):
    idx = html.find('id="' + element_id + '"')
    if idx < 0: return ''
    start = html.rfind('<', 0, idx)
    end = html.find('>', idx)
    return html[start:end + 1] if start >= 0 and end >= 0 else ''

hero_tag = tag_with_id(html, 'sensor-hero')
check("sensor-hero has role=region", 'role="region"' in hero_tag)
check("sensor-hero has aria-label", 'aria-label="Sensor readings"' in hero_tag)

status_tag = tag_with_id(html, 'status-bar')
check("status-bar has role=status", 'role="status"' in status_tag)
check("status-bar has aria-live=polite", 'aria-live="polite"' in status_tag)

health_tag = tag_with_id(html, 'health-strip')
check("health-strip has aria-label", 'aria-label="Connection health"' in health_tag)

toast_tag = tag_with_id(html, 'toast-container')
check("toast-container has role=alert", 'role="alert"' in toast_tag)
check("toast-container has aria-live=assertive", 'aria-live="assertive"' in toast_tag)
check("toast-container has aria-atomic", 'aria-atomic' in toast_tag)

overlay_tag = tag_with_id(html, 'detail-overlay')
check("detail-overlay has role=dialog", 'role="dialog"' in overlay_tag)
check("detail-overlay has aria-modal", 'aria-modal="true"' in overlay_tag)
check("detail-overlay has aria-label", 'aria-label="Sensor detail"' in overlay_tag)

# =====================================================================
#  CSS — focus styles and skip-link
# =====================================================================
print("\n== CSS: Focus and skip-link styles ==")

css_path = os.path.join(BASE, 'css', 'style.css')
with open(css_path, 'r') as f:
    css = f.read()

check(".skip-link style", '.skip-link' in css)
skip_css = css.split('.skip-link')[1].split('}')[0]
check("skip-link off-screen by default", '-9999px' in skip_css)
check("skip-link has z-index", 'z-index' in skip_css)
check("skip-link has background", 'var(--accent)' in skip_css)

check(".skip-link:focus style", '.skip-link:focus' in css)
focus_skip = css.split('.skip-link:focus')[1].split('}')[0]
check("skip-link:focus moves on-screen", 'left' in focus_skip)

check(":focus-visible global style", ':focus-visible' in css)
# Find the global :focus-visible rule (not inside .hero-card)
fv_idx = css.index(':focus-visible')
fv_block = css[fv_idx:].split('}')[0]
check("focus-visible has outline", 'outline' in fv_block)
check("focus-visible uses accent color", 'var(--accent)' in fv_block)
check("focus-visible has outline-offset", 'outline-offset' in fv_block)

check(".hero-card:focus-visible style", '.hero-card:focus-visible' in css)
hero_fv = css.split('.hero-card:focus-visible')[1].split('}')[0]
check("hero-card focus-visible has outline", 'outline' in hero_fv)
check("hero-card focus-visible has box-shadow", 'box-shadow' in hero_fv)
check("hero-card focus-visible uses accent-soft", 'var(--accent-soft)' in hero_fv)

# =====================================================================
#  JavaScript — keyboard navigation and focus management
# =====================================================================
print("\n== JS: Keyboard nav and focus management ==")

js_path = os.path.join(BASE, 'js', 'app.js')
with open(js_path, 'r') as f:
    js = f.read()

# Hero cards have tabindex and role
hero_func = js[js.find('function drawSensorHero'):js.find('function drawStatusBar')]
check("hero cards have tabindex=0", 'tabindex="0"' in hero_func)
check("hero cards have role=button", 'role="button"' in hero_func)
check("hero cards have aria-label", 'aria-label=' in hero_func)

# Keyboard handler for hero cards
init_block = js[js.find('initDetailOverlay'):]
check("keydown listener on heroBox", "heroBox.addEventListener('keydown'" in init_block)
check("Enter key opens detail", "'Enter'" in init_block)
check("Space key opens detail", "' '" in init_block)
check("preventDefault on Space/Enter", 'preventDefault' in init_block)

# Focus management
check("lastFocusedCard variable", 'lastFocusedCard' in init_block)
check("detailCard gets tabindex=-1", "tabindex', '-1'" in init_block or 'tabindex","-1"' in init_block)
check("detailCard.focus on open", 'detailCard.focus()' in init_block)
check("lastFocusedCard.focus on close", 'lastFocusedCard.focus()' in init_block or 'lastFocusedCard) lastFocusedCard.focus()' in init_block)
check("focus restored on Escape", 'lastFocusedCard' in init_block.split('Escape')[1][:200] if 'Escape' in init_block else False)
check("focus restored on backdrop click", 'lastFocusedCard' in init_block.split('e.target === detailOverlay')[1][:200] if 'e.target === detailOverlay' in init_block else False)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*40}")
print(f"  {passed} passed, {failed} failed")
print(f"{'='*40}")
sys.exit(1 if failed else 0)
