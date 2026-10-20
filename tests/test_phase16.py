#!/usr/bin/env python3
"""
Phase 16 tests: dark mode with persistent toggle.
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
#  HTML structure
# =====================================================================
print("\n== HTML: Dark mode toggle ==")

html_path = os.path.join(BASE, 'index.html')
with open(html_path, 'r') as f:
    html = f.read()

check("dark-mode-btn exists", 'id="dark-mode-btn"' in html)
check("dark-mode-btn has icon-btn class", 'class="icon-btn" id="dark-mode-btn"' in html)
check("dark-mode-btn has SVG icon", '<svg' in html.split('dark-mode-btn')[1].split('</button>')[0])
check("dark-mode-btn before settings-btn", html.find('dark-mode-btn') < html.find('settings-btn'))
check("dark-mode-btn has title", 'title="Toggle dark mode"' in html)

# =====================================================================
#  CSS — dark mode token selectors
# =====================================================================
print("\n== CSS: Dark mode tokens ==")

css_path = os.path.join(BASE, 'css', 'style.css')
with open(css_path, 'r') as f:
    css = f.read()

# Base theme: guarded media query
check("base dark: guarded media query", ':root:not([data-theme="light"])' in css)
check("base dark: explicit data-theme selector", ':root[data-theme="dark"]' in css)

# Check that root[data-theme="dark"] has the dark tokens
root_dark = css.split(':root[data-theme="dark"]')[1].split('}')[0]
check("root dark has --bg", '--bg' in root_dark)
check("root dark has --card", '--card' in root_dark)
check("root dark has --text", '--text' in root_dark)
check("root dark has --input-bg", '--input-bg' in root_dark)
check("root dark has color-scheme: dark", 'color-scheme: dark' in root_dark or 'color-scheme:dark' in root_dark)

# Industrial theme
check("industrial dark: guarded media query", 'not([data-theme="light"]) body.theme-industrial' in css)
check("industrial dark: explicit data-theme selector", ':root[data-theme="dark"] body.theme-industrial' in css)

ind_dark = css.split(':root[data-theme="dark"] body.theme-industrial')[1].split('}')[0]
check("industrial dark has --bg", '--bg' in ind_dark)
check("industrial dark has --accent", '--accent' in ind_dark)

# Technical theme
check("technical dark: guarded media query", 'not([data-theme="light"]) body.theme-technical' in css)
check("technical dark: explicit data-theme selector", ':root[data-theme="dark"] body.theme-technical' in css)

tech_dark = css.split(':root[data-theme="dark"] body.theme-technical')[1].split('}')[0]
check("technical dark has --bg", '--bg' in tech_dark)
check("technical dark has --accent", '--accent' in tech_dark)

# Light tokens still on bare :root
root_light = css.split(':root')[1].split('}')[0]
check("base light --bg is light color", '#f4f5f7' in root_light)
check("base light --text is dark color", '#1c1f24' in root_light)

# =====================================================================
#  JavaScript — dark mode functions
# =====================================================================
print("\n== JS: Dark mode logic ==")

js_path = os.path.join(BASE, 'js', 'app.js')
with open(js_path, 'r') as f:
    js = f.read()

check("darkModePref variable", 'darkModePref' in js)
check("ls_dark storage key", "'ls_dark'" in js)
check("isDarkActive function", 'function isDarkActive' in js)
check("applyDarkMode function", 'function applyDarkMode' in js)
check("updateDarkIcon function", 'function updateDarkIcon' in js)
check("toggleDarkMode function", 'function toggleDarkMode' in js)

# isDarkActive logic
is_dark_func = js[js.find('function isDarkActive'):js.find('function applyDarkMode')]
check("isDarkActive checks darkModePref dark", "'dark'" in is_dark_func)
check("isDarkActive checks darkModePref light", "'light'" in is_dark_func)
check("isDarkActive checks prefers-color-scheme", 'prefers-color-scheme' in is_dark_func)

# applyDarkMode logic
apply_func = js[js.find('function applyDarkMode'):js.find('function updateDarkIcon')]
check("applyDarkMode sets data-theme attribute", "setAttribute('data-theme'" in apply_func or 'setAttribute("data-theme"' in apply_func)
check("applyDarkMode removes data-theme", "removeAttribute('data-theme')" in apply_func or 'removeAttribute("data-theme")' in apply_func)
check("applyDarkMode sets on documentElement", 'document.documentElement' in apply_func)
check("applyDarkMode calls updateDarkIcon", 'updateDarkIcon' in apply_func)

# updateDarkIcon logic
icon_func = js[js.find('function updateDarkIcon'):js.find('function toggleDarkMode')]
check("updateDarkIcon has sun SVG path", 'M12 3v1' in icon_func)
check("updateDarkIcon has moon SVG path", 'M21 12.79' in icon_func)
check("updateDarkIcon swaps innerHTML", 'innerHTML' in icon_func)

# toggleDarkMode logic
toggle_func = js[js.find('function toggleDarkMode'):js.find('\n\n', js.find('function toggleDarkMode'))]
check("toggleDarkMode calls isDarkActive", 'isDarkActive' in toggle_func)
check("toggleDarkMode saves to localStorage", 'localStorage.setItem' in toggle_func)
check("toggleDarkMode calls applyDarkMode", 'applyDarkMode' in toggle_func)
check("toggleDarkMode flips preference", "'light'" in toggle_func and "'dark'" in toggle_func)

# Initial call
check("applyDarkMode called on load", '\napplyDarkMode();' in js or '\rapplyDarkMode();' in js)

# System preference listener
check("matchMedia change listener", "addEventListener('change'" in js or 'addEventListener("change"' in js)

# localStorage read on init
check("localStorage read for ls_dark", "localStorage.getItem('ls_dark')" in js)

# try/catch around localStorage
dark_init = js[js.find('Phase 16'):js.find('var pollTimer')]
check("try/catch around localStorage init", 'try' in dark_init and 'catch' in dark_init)

# Button handler
check("dark-mode-btn onclick handler", "getElementById('dark-mode-btn')" in js)
check("toggleDarkMode bound to button", "dark-mode-btn').onclick = toggleDarkMode" in js)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*40}")
print(f"  {passed} passed, {failed} failed")
print(f"{'='*40}")
sys.exit(1 if failed else 0)
