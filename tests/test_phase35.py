#!/usr/bin/env python3
"""
Phase 35 tests: API Security — input limits, rate limiting, security headers, factory reset.
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

GW = os.path.join(os.path.dirname(__file__), '..', 'firmware', 'gateway-esp32')
DATA = os.path.join(GW, 'data')

# =====================================================================
#  auth.h: Rate limiting fields and logic
# =====================================================================
print("\n== Phase 35: Rate limiting in auth.h ==")

with open(os.path.join(GW, 'auth.h'), 'r', encoding='utf-8') as f:
    auth_h = f.read()

check("AuthManager has failedAttempts field",
      'failedAttempts' in auth_h and 'uint8_t' in auth_h.split('failedAttempts')[0].split('\n')[-1])
check("AuthManager has lockoutUntil field",
      'lockoutUntil' in auth_h and 'uint32_t' in auth_h.split('lockoutUntil')[0].split('\n')[-1])
check("failedAttempts initialized to 0",
      'failedAttempts(0)' in auth_h)
check("lockoutUntil initialized to 0",
      'lockoutUntil(0)' in auth_h)
check("429 status on lockout",
      '429' in auth_h and 'too many attempts' in auth_h)
check("Lockout threshold is 5 attempts",
      'failedAttempts >= 5' in auth_h)
check("Lockout duration is 60 seconds",
      '60000' in auth_h)
check("Failed attempts increment on auth failure",
      'failedAttempts++' in auth_h)
check("Successful auth resets failedAttempts",
      'failedAttempts = 0' in auth_h)

# =====================================================================
#  gateway-esp32.ino: checkBodySize helper
# =====================================================================
print("\n== Phase 35: Body size validation ==")

with open(os.path.join(GW, 'gateway-esp32.ino'), 'r', encoding='utf-8') as f:
    ino = f.read()

check("checkBodySize function exists",
      'bool checkBodySize(' in ino)
check("Returns 413 on oversized body",
      '413' in ino and 'too large' in ino)
check("Default limit is 2048 bytes",
      '2048' in ino and 'checkBodySize' in ino)

# Count how many handlers call checkBodySize
body_check_count = ino.count('checkBodySize()')  + ino.count('checkBodySize(4096)') + ino.count('checkBodySize(8192)')
check(f"checkBodySize called in multiple handlers ({body_check_count} found)",
      body_check_count >= 10)

check("Config restore uses larger limit (8192)",
      'checkBodySize(8192)' in ino)

# =====================================================================
#  gateway-esp32.ino: Security headers
# =====================================================================
print("\n== Phase 35: Security headers ==")

check("addSecHeaders function exists",
      'void addSecHeaders()' in ino)
check("X-Content-Type-Options: nosniff header",
      'X-Content-Type-Options' in ino and 'nosniff' in ino)
check("X-Frame-Options: DENY header",
      'X-Frame-Options' in ino and 'DENY' in ino)
check("addSecHeaders called in handleState",
      # Find addSecHeaders between handleState and next handler
      re.search(r'void handleState\(\).*?addSecHeaders\(\)', ino, re.DOTALL) is not None)
check("addSecHeaders called in handleGetConfig",
      re.search(r'void handleGetConfig\(\).*?addSecHeaders\(\)', ino, re.DOTALL) is not None)

# =====================================================================
#  gateway-esp32.ino: Factory reset
# =====================================================================
print("\n== Phase 35: Factory reset ==")

check("handleFactoryReset function exists",
      'void handleFactoryReset()' in ino)
check("Factory reset requires ROLE_TECHNICIAN",
      re.search(r'handleFactoryReset.*?ROLE_TECHNICIAN', ino, re.DOTALL) is not None)
check("Factory reset deletes config files",
      'env.json' in ino and 'auth.json' in ino and 'LittleFS.remove' in ino)
check("Factory reset logs to eventLog",
      re.search(r'handleFactoryReset.*?eventLog\.add', ino, re.DOTALL) is not None)
check("Factory reset calls ESP.restart()",
      re.search(r'handleFactoryReset.*?ESP\.restart', ino, re.DOTALL) is not None)
check("Factory reset route registered",
      '/api/factory-reset' in ino and 'handleFactoryReset' in ino)

# =====================================================================
#  Dashboard: Factory reset UI
# =====================================================================
print("\n== Phase 35: Dashboard factory reset UI ==")

with open(os.path.join(DATA, 'index.html'), 'r', encoding='utf-8') as f:
    html = f.read()

with open(os.path.join(DATA, 'js', 'app.js'), 'r', encoding='utf-8') as f:
    js = f.read()

check("factory-reset-btn in HTML",
      'id="factory-reset-btn"' in html)
check("factory-reset-section hidden by default",
      'factory-reset-section' in html and 'display:none' in html.split('factory-reset-section')[1][:100])
check("JS shows factory-reset-section for technicians",
      'factory-reset-section' in js and "display = ''" in js.split('factory-reset-section')[1][:50])
check("JS factory reset uses double confirmation",
      js.count('confirm(') >= 2 and 'factory-reset' in js.lower())
check("JS calls /api/factory-reset endpoint",
      '/api/factory-reset' in js)
check("JS handles factory reset response",
      'factory-reset-status' in js)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 35 results: {passed} passed, {failed} failed")
if failed:
    print("SOME TESTS FAILED")
    sys.exit(1)
else:
    print("ALL TESTS PASSED")
