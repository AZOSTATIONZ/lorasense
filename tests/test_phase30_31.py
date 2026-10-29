#!/usr/bin/env python3
"""
Phase 30-31 tests: CSV export for action/event logs, system info enhancements,
firmware version bump to 3.0.0.
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

GW = os.path.join(os.path.dirname(__file__), '..', 'firmware', 'gateway-esp32')
DATA = os.path.join(GW, 'data')

# =====================================================================
#  Phase 30: CSV exports
# =====================================================================
print("\n== Phase 30: CSV exports ==")

with open(os.path.join(DATA, 'js', 'app.js'), 'r', encoding='utf-8') as f:
    js = f.read()

with open(os.path.join(DATA, 'index.html'), 'r', encoding='utf-8') as f:
    html = f.read()

check("downloadCsv helper exists", 'function downloadCsv' in js)
check("export-action-log-btn in HTML", 'id="export-action-log-btn"' in html)
check("export-event-log-btn in HTML", 'id="export-event-log-btn"' in html)
check("action log export handler", "export-action-log-btn" in js)
check("event log export handler", "export-event-log-btn" in js)
check("action log fetches /api/action-log", "'/api/action-log'" in js)
check("event log fetches /api/events", "'/api/events" in js)
check("action CSV has header row", "uptime_s,rule,action,equipment,value" in js)
check("event CSV has header row", "uptime_s,category,message" in js)
check("uses downloadCsv for action log", "downloadCsv(" in js)
check("existing sensor CSV export intact", "function exportCsv()" in js)

# =====================================================================
#  Phase 31: System info enhancements
# =====================================================================
print("\n== Phase 31: System info + version bump ==")

with open(os.path.join(GW, 'gateway-esp32.ino'), 'r', encoding='utf-8') as f:
    ino = f.read()

check("firmware version 3.0.0", '"3.0.0"' in ino)
check("system API reports nodes", 'doc["nodes"]' in ino)
check("system API reports wifi_clients", 'doc["wifi_clients"]' in ino)
check("system API reports clock_valid", 'doc["clock_valid"]' in ino)
check("system API reports lora_addr", 'doc["lora_addr"]' in ino)
check("dashboard shows nodes count", "Nodes" in js and "d.nodes" in js)
check("dashboard shows WiFi clients", "WiFi Clients" in js and "d.wifi_clients" in js)
check("dashboard shows clock status", "d.clock_valid" in js)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 30-31 results: {passed} passed, {failed} failed")
if failed:
    print("SOME TESTS FAILED")
    sys.exit(1)
else:
    print("ALL TESTS PASSED")
