#!/usr/bin/env python3
"""
Phase 28 tests: time-based automation rules.
Verifies schedule rule support in config, firmware logic, API handlers, and dashboard UI.
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
#  config.h: AutomationRule schedule fields
# =====================================================================
print("\n== config.h: Schedule fields ==")

with open(os.path.join(GW, 'config.h'), 'r', encoding='utf-8') as f:
    cfg = f.read()

check("RULE_SENSOR constant", 'RULE_SENSOR' in cfg and '= 0' in cfg)
check("RULE_SCHEDULE constant", 'RULE_SCHEDULE' in cfg and '= 1' in cfg)
check("ruleType field in struct", 'ruleType' in cfg)
check("schedStartMin field", 'schedStartMin' in cfg)
check("schedStopMin field", 'schedStopMin' in cfg)
check("schedDays bitmask field", 'schedDays' in cfg)
check("loadRules reads rule_type", 'rule_type' in cfg and 'ruleType' in cfg)
check("saveRules writes sched_start", 'sched_start' in cfg)
check("saveRules writes sched_stop", 'sched_stop' in cfg)
check("saveRules writes sched_days", 'sched_days' in cfg)
check("addRule has ruleType param", re.search(r'addRule\(.*ruleType', cfg) is not None)
check("updateRule has ruleType param", re.search(r'updateRule\(.*ruleType', cfg) is not None)

# =====================================================================
#  gateway-esp32.ino: Software clock
# =====================================================================
print("\n== gateway-esp32.ino: Software clock ==")

with open(os.path.join(GW, 'gateway-esp32.ino'), 'r', encoding='utf-8') as f:
    ino = f.read()

check("clockValid variable", 'clockValid' in ino and 'bool' in ino)
check("clockSetAt variable", 'uint32_t clockSetAt' in ino)
check("clockBaseMin variable", 'uint32_t clockBaseMin' in ino)
check("clockDay variable", 'clockDay' in ino and 'uint8_t' in ino)
check("setGatewayClock function", 'void setGatewayClock(' in ino)
check("gatewayMinuteOfDay function", 'uint16_t gatewayMinuteOfDay()' in ino)
check("gatewayDayOfWeek function", 'uint8_t gatewayDayOfWeek()' in ino)
check("clock wraps at 1440 minutes", '% 1440' in ino)
check("day wraps at 7", '% 7' in ino)

# =====================================================================
#  gateway-esp32.ino: evaluateRules schedule branch
# =====================================================================
print("\n== gateway-esp32.ino: Schedule rule evaluation ==")

check("RULE_SCHEDULE branch in evaluateRules", 'r.ruleType == RULE_SCHEDULE' in ino)
check("checks clockValid for schedule", 'clockValid' in ino)
check("gets gatewayMinuteOfDay", 'gatewayMinuteOfDay()' in ino)
check("gets gatewayDayOfWeek", 'gatewayDayOfWeek()' in ino)
check("day bitmask check", 'schedDays >> nowDay' in ino or 'r.schedDays' in ino)
check("overnight window support (stop < start)", 'r.schedStartMin <= r.schedStopMin' in ino or 'schedStartMin <= r.schedStopMin' in ino)
check("SCHED_ON action log", 'SCHED_ON' in ino)
check("SCHED_OFF action log", 'SCHED_OFF' in ino)
check("schedule branch continues past sensor logic", re.search(r'SCHED_OFF.*?continue;', ino, re.DOTALL) is not None)

# =====================================================================
#  gateway-esp32.ino: Clock API
# =====================================================================
print("\n== gateway-esp32.ino: Clock API ==")

check("handleGetClock function", 'void handleGetClock()' in ino)
check("handleSetClock function", 'void handleSetClock()' in ino)
check("GET /api/clock route", '"/api/clock"' in ino and 'HTTP_GET' in ino)
check("POST /api/clock route", '"/api/clock"' in ino and 'handleSetClock' in ino)
check("clock validates hour 0-23", 'hour > 23' in ino or 'hour < 0' in ino)
check("clock validates minute 0-59", 'minute > 59' in ino or 'minute < 0' in ino)
check("clock validates day 0-6", 'day > 6' in ino or 'day < 0' in ino)
check("calls setGatewayClock on POST", 'setGatewayClock' in ino)
check("clock event log", "Clock set to" in ino)

# =====================================================================
#  gateway-esp32.ino: handleState includes clock
# =====================================================================
print("\n== gateway-esp32.ino: State includes clock ==")

check("state has clock_valid", 'clock_valid' in ino)
check("state has clock_hour", 'clock_hour' in ino)
check("state has clock_minute", 'clock_minute' in ino)
check("state has clock_day", 'clock_day' in ino)

# =====================================================================
#  gateway-esp32.ino: Rule API includes schedule fields
# =====================================================================
print("\n== gateway-esp32.ino: Rule API schedule fields ==")

check("handleGetRules returns rule_type", 'obj["rule_type"]' in ino or "obj[\"rule_type\"]" in ino)
check("handleGetRules returns sched_start", 'obj["sched_start"]' in ino or "obj[\"sched_start\"]" in ino)
check("handleAddRule reads rule_type", re.search(r'handleAddRule.*?doc\["rule_type"\]', ino, re.DOTALL) is not None)
check("handleAddRule reads sched_start", re.search(r'handleAddRule.*?doc\["sched_start"\]', ino, re.DOTALL) is not None)
check("handleUpdateRule reads rule_type", re.search(r'handleUpdateRule.*?doc\["rule_type"\]', ino, re.DOTALL) is not None)

# =====================================================================
#  Dashboard HTML: Rule form schedule fields
# =====================================================================
print("\n== HTML: Rule form schedule fields ==")

with open(os.path.join(DATA, 'index.html'), 'r', encoding='utf-8') as f:
    html = f.read()

check("rule-type select", 'id="rule-type"' in html)
check("sensor-fields wrapper", 'id="sensor-fields"' in html)
check("schedule-fields wrapper", 'id="schedule-fields"' in html)
check("schedule start time input", 'id="rule-sched-start"' in html)
check("schedule stop time input", 'id="rule-sched-stop"' in html)
check("day checkboxes container", 'id="rule-sched-days"' in html)
check("7 day checkboxes", html.count('type="checkbox"') >= 7)
check("clock section", 'id="clock-section"' in html)
check("set-clock-btn", 'id="set-clock-btn"' in html)
check("clock-status display", 'id="clock-status"' in html)

# =====================================================================
#  Dashboard JS: Schedule rule support
# =====================================================================
print("\n== JS: Schedule rule support ==")

with open(os.path.join(DATA, 'js', 'app.js'), 'r', encoding='utf-8') as f:
    js = f.read()

check("pad2 helper", 'function pad2' in js)
check("toggleRuleTypeFields function", 'function toggleRuleTypeFields' in js)
check("updateClockStatus function", 'function updateClockStatus' in js)
check("clock sync button handler", "'/api/clock'" in js)
check("addRule sends rule_type", 'rule_type: ruleType' in js)
check("addRule sends sched_start", 'data.sched_start' in js)
check("addRule sends sched_days", 'data.sched_days' in js)
check("toggleRule sends rule_type", 'rule_type: rule.rule_type' in js)
check("renderRules shows schedule time", "sched_start" in js)
check("renderRules shows day labels", "dayLabels" in js)
check("schedule type badge in rule list", 'rule-type-badge' in js)
check("clock icon for schedule active rule", "rule_type === 1" in js)
check("updateClockStatus called from poll", "updateClockStatus(s)" in js)

# =====================================================================
#  CSS: Schedule UI styles
# =====================================================================
print("\n== CSS: Schedule UI styles ==")

with open(os.path.join(DATA, 'css', 'style.css'), 'r', encoding='utf-8') as f:
    css = f.read()

check("sched-days flex layout", '.sched-days' in css)
check("rule-type-badge style", '.rule-type-badge' in css)
check("schedule badge color", '.rule-type-badge.sched' in css)
check("clock-status style", '.clock-status' in css)
check("clock-ok style", '.clock-ok' in css)
check("clock-warn style", '.clock-warn' in css)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 28 results: {passed} passed, {failed} failed")
if failed:
    print("SOME TESTS FAILED")
    sys.exit(1)
else:
    print("ALL TESTS PASSED")
