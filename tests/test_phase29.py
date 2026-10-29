#!/usr/bin/env python3
"""
Phase 29 tests: dashboard alert toasts via state diffing.
Verifies diffAndNotify function and toast integration in the poll cycle.
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

DATA = os.path.join(os.path.dirname(__file__), '..', 'firmware', 'gateway-esp32', 'data')

# =====================================================================
#  JS: diffAndNotify function
# =====================================================================
print("\n== JS: State-diff alert toasts ==")

with open(os.path.join(DATA, 'js', 'app.js'), 'r', encoding='utf-8') as f:
    js = f.read()

check("diffAndNotify function exists", 'function diffAndNotify' in js)
check("called from refresh cycle", 'diffAndNotify(lastState, s)' in js)
check("skips if no previous state", 'if (!prev) return' in js)
check("diffs rule triggered status", "r.triggered && !pr.triggered" in js)
check("diffs rule release", "!r.triggered && pr.triggered" in js)
check("diffs fault appeared", "r.fault && !pr.fault" in js)
check("diffs fault cleared", "!r.fault && pr.fault" in js)
check("shows equipment name for triggered rule", "eqName" in js)
check("differentiates schedule vs sensor rule", "r.rule_type === 1" in js)
check("diffs node online/offline", "next.node_online" in js and "prev.node_online" in js)
check("node offline toast", "Node offline" in js)
check("node online toast", "Node back online" in js)
check("clock set toast", "clock set" in js or "clock_valid" in js)
check("clock lost toast", "clock lost" in js or "schedule rules paused" in js)
check("uses showToast for alerts", "showToast(" in js)
check("uses warning type for rule trigger", "'warning'" in js)
check("uses error type for fault", "'error'" in js)
check("uses success type for recovery", "'success'" in js)
check("uses info type for release", "'info'" in js)

# =====================================================================
#  Existing toast infrastructure intact
# =====================================================================
print("\n== Toast infrastructure ==")

check("showToast function exists", 'function showToast' in js)
check("toast container referenced", "getElementById('toast-container')" in js)

with open(os.path.join(DATA, 'index.html'), 'r', encoding='utf-8') as f:
    html = f.read()

check("toast container in HTML", 'id="toast-container"' in html)

with open(os.path.join(DATA, 'css', 'style.css'), 'r', encoding='utf-8') as f:
    css = f.read()

check("toast CSS exists", '.toast-container' in css)
check("toast success style", '.toast-success' in css)
check("toast error style", '.toast-error' in css)
check("toast warning style", '.toast-warning' in css)
check("toast info style", '.toast-info' in css)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 29 results: {passed} passed, {failed} failed")
if failed:
    print("SOME TESTS FAILED")
    sys.exit(1)
else:
    print("ALL TESTS PASSED")
