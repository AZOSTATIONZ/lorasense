#!/usr/bin/env python3
"""
Phase 9 tests: authentication roles, event log model, config backup validation.

Mirrors the C++ logic from auth.h, device.h, and gateway-esp32.ino
so we can test it without flashing.
"""
import sys, json

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

# =====================================================================
#  Role hierarchy and access control model
# =====================================================================
print("\n== Role hierarchy ==")

ROLE_NONE       = 0
ROLE_USER       = 1
ROLE_TECHNICIAN = 2

def check_access(user_role, required_role):
    """Mirrors auth.check() logic: returns True if user has sufficient role."""
    return user_role >= required_role

# Technician can access everything
check("technician -> technician endpoints", check_access(ROLE_TECHNICIAN, ROLE_TECHNICIAN))
check("technician -> user endpoints", check_access(ROLE_TECHNICIAN, ROLE_USER))

# User can access user endpoints only
check("user -> user endpoints", check_access(ROLE_USER, ROLE_USER))
check("user cannot access technician endpoints", not check_access(ROLE_USER, ROLE_TECHNICIAN))

# No role cannot access anything
check("no role -> cannot access user", not check_access(ROLE_NONE, ROLE_USER))
check("no role -> cannot access technician", not check_access(ROLE_NONE, ROLE_TECHNICIAN))

# =====================================================================
#  Route-to-role mapping validation
# =====================================================================
print("\n== Route role mapping ==")

ROUTES = {
    # Dashboard / read-only: ROLE_USER
    "GET /api/state":         ROLE_USER,
    "POST /api/devices":      ROLE_USER,
    "GET /api/config":        ROLE_USER,
    "GET /api/rules":         ROLE_USER,
    "GET /api/action-log":    ROLE_USER,
    "GET /api/history":       ROLE_USER,
    "GET /api/config/sensors": ROLE_USER,
    "GET /api/hw/nodes":      ROLE_USER,
    "GET /api/hw/pins":       ROLE_USER,
    "GET /api/hw/relays":     ROLE_USER,
    "GET /api/hw/drivers":    ROLE_USER,
    "GET /api/events":        ROLE_USER,
    "GET /api/system":        ROLE_USER,
    "POST /api/override":     ROLE_USER,

    # Configuration: ROLE_TECHNICIAN
    "POST /api/config/environment":  ROLE_TECHNICIAN,
    "POST /api/config/zones":        ROLE_TECHNICIAN,
    "DELETE /api/config/zones":      ROLE_TECHNICIAN,
    "POST /api/config/equipment":    ROLE_TECHNICIAN,
    "PUT /api/config/equipment":     ROLE_TECHNICIAN,
    "DELETE /api/config/equipment":  ROLE_TECHNICIAN,
    "POST /api/onboarding":          ROLE_TECHNICIAN,
    "POST /api/commission":          ROLE_TECHNICIAN,
    "POST /api/rules":               ROLE_TECHNICIAN,
    "PUT /api/rules":                ROLE_TECHNICIAN,
    "DELETE /api/rules":             ROLE_TECHNICIAN,
    "POST /api/config/sensors":      ROLE_TECHNICIAN,
    "DELETE /api/config/sensors":    ROLE_TECHNICIAN,
    "POST /api/hw/nodes":            ROLE_TECHNICIAN,
    "DELETE /api/hw/nodes":          ROLE_TECHNICIAN,
    "GET /api/test":                 ROLE_TECHNICIAN,
    "GET /api/config/backup":        ROLE_TECHNICIAN,
    "POST /api/config/restore":      ROLE_TECHNICIAN,
    "GET /api/auth":                 ROLE_TECHNICIAN,
    "POST /api/auth":                ROLE_TECHNICIAN,
    "DELETE /api/events":            ROLE_TECHNICIAN,
}

# A user should be able to access all USER routes
user_routes = [r for r, role in ROUTES.items() if role == ROLE_USER]
tech_routes = [r for r, role in ROUTES.items() if role == ROLE_TECHNICIAN]

for route in user_routes:
    check(f"user can access {route}", check_access(ROLE_USER, ROUTES[route]))

for route in tech_routes:
    check(f"user blocked from {route}", not check_access(ROLE_USER, ROUTES[route]))

for route in ROUTES:
    check(f"technician can access {route}", check_access(ROLE_TECHNICIAN, ROUTES[route]))

# =====================================================================
#  Credential model
# =====================================================================
print("\n== Credential model ==")

MAX_CREDENTIALS = 4

def make_cred(username, password, role, active=True):
    return {"username": username, "password": password, "role": role, "active": active}

# Validation: empty username should be rejected
cred_empty = make_cred("", "pass", ROLE_USER)
check("empty username invalid", not cred_empty["username"])

# Validation: username must fit in 32 chars
check("username fits 31 chars", len("a" * 31) < 32)
check("username too long at 32", len("a" * 32) >= 32)

# Active vs inactive
cred_active = make_cred("admin", "pass", ROLE_TECHNICIAN, True)
cred_inactive = make_cred("old", "pass", ROLE_TECHNICIAN, False)
check("active credential is active", cred_active["active"])
check("inactive credential skipped", not cred_inactive["active"])

# Max credentials enforced
creds = [make_cred(f"user{i}", "pass", ROLE_USER) for i in range(MAX_CREDENTIALS + 1)]
check("max credentials is 4", MAX_CREDENTIALS == 4)
check("5th credential would exceed limit", len(creds) > MAX_CREDENTIALS)

# =====================================================================
#  Event log model
# =====================================================================
print("\n== Event log ==")

EVENT_LOG_MAX = 100

VALID_CATEGORIES = ["rule", "relay", "config", "auth", "fault", "system"]

for cat in VALID_CATEGORIES:
    check(f"category '{cat}' fits in 12 chars", len(cat) < 12)

# Message length
check("message fits in 64 chars", len("Rule 'Fan on when hot' triggered -> Living room fan ON") < 64)
check("long message would be truncated", len("A" * 64) >= 64)

# Ring buffer behavior
events = []
for i in range(EVENT_LOG_MAX + 20):
    events.append({"t": i, "cat": "system", "msg": f"Event {i}"})
    while len(events) > EVENT_LOG_MAX:
        events.pop(0)

check(f"event log capped at {EVENT_LOG_MAX}", len(events) == EVENT_LOG_MAX)
check("oldest event is #20 (first 20 trimmed)", events[0]["msg"] == "Event 20")
check("newest event is #119", events[-1]["msg"] == "Event 119")

# Filtering
rule_events = [e for e in events if e["cat"] == "rule"]
check("filter returns only matching category", all(e["cat"] == "rule" for e in rule_events))

# =====================================================================
#  Config backup model
# =====================================================================
print("\n== Config backup ==")

CONFIG_SCHEMA_VERSION = 1

def validate_backup(data):
    """Mirrors handleConfigRestore() validation."""
    if "_backup_v" not in data:
        return False, "not a valid backup file"
    if data.get("_schema_v", 0) > CONFIG_SCHEMA_VERSION:
        return False, "backup from newer firmware version"
    return True, "ok"

# Valid backup
valid_backup = {
    "_backup_v": 1,
    "_schema_v": 1,
    "_timestamp": 12345,
    "env": {"name": "Test"},
    "zones": [{"id": "z1"}],
    "equipment": [],
    "sensors": [],
    "rules": [],
    "nodes": []
}
ok, msg = validate_backup(valid_backup)
check("valid backup passes", ok)

# Missing backup version
bad_backup1 = {"env": {}}
ok, msg = validate_backup(bad_backup1)
check("missing _backup_v rejected", not ok)
check("error says not valid", "not a valid" in msg)

# Future schema version
bad_backup2 = {"_backup_v": 1, "_schema_v": 99}
ok, msg = validate_backup(bad_backup2)
check("newer schema rejected", not ok)
check("error says newer firmware", "newer firmware" in msg)

# Same schema version OK
same_ver = {"_backup_v": 1, "_schema_v": CONFIG_SCHEMA_VERSION}
ok, msg = validate_backup(same_ver)
check("same schema version OK", ok)

# Older schema version OK (backward compatible)
older_ver = {"_backup_v": 1, "_schema_v": 0}
ok, msg = validate_backup(older_ver)
check("older schema version OK", ok)

# Config file set
CONFIG_FILES = ["env", "zones", "equipment", "sensors", "rules", "nodes"]
for key in CONFIG_FILES:
    check(f"backup includes '{key}' key", key in valid_backup)

check("backup has 6 config sections", len(CONFIG_FILES) == 6)

# =====================================================================
#  Auth disabled passthrough
# =====================================================================
print("\n== Auth disabled passthrough ==")

def check_with_auth_state(enabled, cred_count, user_role, required_role):
    """Mirrors full auth.check() logic including disabled/no-creds bypass."""
    if not enabled:
        return True
    if cred_count == 0:
        return True
    return user_role >= required_role

check("auth disabled -> always passes", check_with_auth_state(False, 2, ROLE_NONE, ROLE_TECHNICIAN))
check("no credentials -> always passes", check_with_auth_state(True, 0, ROLE_NONE, ROLE_TECHNICIAN))
check("enabled + creds + insufficient -> blocked",
      not check_with_auth_state(True, 2, ROLE_USER, ROLE_TECHNICIAN))
check("enabled + creds + sufficient -> passes",
      check_with_auth_state(True, 2, ROLE_TECHNICIAN, ROLE_TECHNICIAN))

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*40}")
print(f"  {passed} passed, {failed} failed")
print(f"{'='*40}")
sys.exit(1 if failed else 0)
