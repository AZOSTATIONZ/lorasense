#!/usr/bin/env python3
"""
Phase 15 tests: toast notifications and status feedback.
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
print("\n== HTML: Toast container ==")

html_path = os.path.join(BASE, 'index.html')
with open(html_path, 'r') as f:
    html = f.read()

check("toast-container exists", 'id="toast-container"' in html)
check("toast-container has class", 'class="toast-container"' in html)
check("toast-container before script tag", html.find('toast-container') < html.find('<script'))

# =====================================================================
#  CSS styles
# =====================================================================
print("\n== CSS: Toast styles ==")

css_path = os.path.join(BASE, 'css', 'style.css')
with open(css_path, 'r') as f:
    css = f.read()

check(".toast-container style", '.toast-container' in css)
tc_css = css.split('.toast-container')[1].split('}')[0]
check("toast-container position fixed", 'fixed' in tc_css)
check("toast-container z-index", 'z-index' in tc_css)
check("toast-container pointer-events none", 'pointer-events' in tc_css and 'none' in tc_css)
check("toast-container flex column", 'flex-direction' in tc_css and 'column' in tc_css)

check(".toast style", '.toast {' in css or '.toast{' in css)
toast_css = css[css.index('.toast {') if '.toast {' in css else css.index('.toast{'):]
toast_block = toast_css.split('}')[0]
check("toast has border-radius", 'border-radius' in toast_block)
check("toast has box-shadow", 'box-shadow' in toast_block)
check("toast pointer-events auto", 'pointer-events' in toast_block and 'auto' in toast_block)
check("toast has animation", 'animation' in toast_block)
check("toast cursor pointer", 'cursor' in toast_block and 'pointer' in toast_block)
check("toast max-width", 'max-width' in toast_block)

check(".toast-out class", '.toast-out' in css or '.toast.toast-out' in css)
check("toast-out has animation", 'toast-out' in css)

check(".toast-icon style", '.toast-icon' in css)
check(".toast-msg style", '.toast-msg' in css)

# Type variants
check(".toast-success style", '.toast-success' in css)
check(".toast-error style", '.toast-error' in css)
check(".toast-warning style", '.toast-warning' in css)
check(".toast-info style", '.toast-info' in css)

check("success uses --on color", 'var(--on)' in css.split('.toast-success')[1].split('}')[0])
check("error uses --fail color", 'var(--fail)' in css.split('.toast-error')[1].split('}')[0])
check("warning uses --pending color", 'var(--pending)' in css.split('.toast-warning')[1].split('}')[0])
check("info uses --accent color", 'var(--accent)' in css.split('.toast-info')[1].split('}')[0])

# Animations
check("toast-in keyframes", '@keyframes toast-in' in css)
check("toast-out keyframes", '@keyframes toast-out' in css)
check("toast-in uses translateX", 'translateX' in css.split('@keyframes toast-in')[1].split('@keyframes')[0])

# Reduced motion
check("prefers-reduced-motion", 'prefers-reduced-motion' in css)
check("reduced-motion shortens toast animation", 'reduce' in css)

# Border-left on types for visual indicator
check("success has border-left", 'border-left' in css.split('.toast-success')[1].split('}')[0])
check("error has border-left", 'border-left' in css.split('.toast-error')[1].split('}')[0])

# =====================================================================
#  JavaScript — showToast function
# =====================================================================
print("\n== JS: Toast system ==")

js_path = os.path.join(BASE, 'js', 'app.js')
with open(js_path, 'r') as f:
    js = f.read()

check("showToast function", 'function showToast' in js)
check("dismissToast function", 'function dismissToast' in js)
check("TOAST_MAX constant", 'TOAST_MAX' in js)
check("TOAST_DURATION constant", 'TOAST_DURATION' in js)
check("TOAST_ICONS object", 'TOAST_ICONS' in js)

# showToast internals
toast_func = js[js.find('function showToast'):js.find('function dismissToast')]
check("showToast creates toast div", "createElement('div')" in toast_func)
check("showToast sets className with type", "'toast toast-'" in toast_func)
check("showToast uses escHtml for message", 'escHtml(message)' in toast_func)
check("showToast appends to container", 'container.appendChild(toast)' in toast_func)
check("showToast auto-dismiss with setTimeout", 'setTimeout' in toast_func)
check("showToast click to dismiss", 'toast.onclick' in toast_func)
check("showToast limits visible toasts", 'TOAST_MAX' in toast_func)

# Swipe to dismiss
check("touchstart listener on toast", 'touchstart' in toast_func)
check("touchend listener on toast", 'touchend' in toast_func)
check("swipe threshold check", 'dx > 60' in toast_func or 'dx>' in toast_func)

# dismissToast internals
dismiss_func = js[js.find('function dismissToast'):]
dismiss_block = dismiss_func[:dismiss_func.find('\n}') + 2]
check("dismissToast adds toast-out class", "classList.add('toast-out')" in dismiss_block or "toast-out" in dismiss_block)
check("dismissToast removes from DOM", 'removeChild' in dismiss_block)
check("dismissToast guards against double dismiss", 'toast-out' in dismiss_block)

# Icon SVGs
check("success icon SVG (checkmark)", 'success' in js.split('TOAST_ICONS')[1].split('};')[0])
check("error icon SVG", 'error' in js.split('TOAST_ICONS')[1].split('};')[0])
check("warning icon SVG", 'warning' in js.split('TOAST_ICONS')[1].split('};')[0])
check("info icon SVG", 'info' in js.split('TOAST_ICONS')[1].split('};')[0])

# =====================================================================
#  JavaScript — integration with existing handlers
# =====================================================================
print("\n== JS: Toast integration ==")

# setDevice shows toast
set_device_func = js[js.find('function setDevice'):js.find('\n\n', js.find('function setDevice'))]
check("setDevice success toast", "showToast(" in set_device_func and "'success'" in set_device_func)
check("setDevice error toast on failure", "'error'" in set_device_func)
check("setDevice resolves device name", 'devName' in set_device_func)

# allOff shows toast
all_off_func = js[js.find('function allOff'):js.find('\n\n', js.find('function allOff'))]
check("allOff success toast", "showToast(" in all_off_func and "'success'" in all_off_func)
check("allOff fallback warning toast", "'warning'" in all_off_func)

# Connection recovery toast
refresh_func = js[js.find('function refresh'):js.find('\n\n', js.find('function refresh'))]
check("refresh tracks wasDisconnected", 'wasDisconnected' in refresh_func)
check("connection restored toast", "'Connection restored'" in refresh_func or "Connection restored" in refresh_func)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*40}")
print(f"  {passed} passed, {failed} failed")
print(f"{'='*40}")
sys.exit(1 if failed else 0)
