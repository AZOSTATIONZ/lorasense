#!/usr/bin/env python3
"""
Phase 34 tests: Duplicate detection + config validation.
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

# =====================================================================
#  device.h: NodeState duplicate detection fields
# =====================================================================
print("\n== Phase 34: NodeState duplicate detection fields ==")

with open(os.path.join(GW, 'device.h'), 'r', encoding='utf-8') as f:
    device_h = f.read()

check("NodeState has lastTelemetrySeq field",
      'lastTelemetrySeq' in device_h and 'uint8_t' in device_h.split('lastTelemetrySeq')[0].split('\n')[-1])
check("NodeState has lastHeartbeatSeq field",
      'lastHeartbeatSeq' in device_h and 'uint8_t' in device_h.split('lastHeartbeatSeq')[0].split('\n')[-1])
check("lastTelemetrySeq initialized to 0xFF in reset()",
      'lastTelemetrySeq = 0xFF' in device_h)
check("lastHeartbeatSeq initialized to 0xFF in reset()",
      'lastHeartbeatSeq = 0xFF' in device_h)

# =====================================================================
#  gateway-esp32.ino: Duplicate detection in checkIncoming
# =====================================================================
print("\n== Phase 34: Duplicate detection in checkIncoming ==")

with open(os.path.join(GW, 'gateway-esp32.ino'), 'r', encoding='utf-8') as f:
    ino = f.read()

check("Duplicate telemetry check compares seq",
      'lastTelemetrySeq' in ino and 'hdr->seq' in ino)
check("Duplicate heartbeat check compares seq",
      'lastHeartbeatSeq' in ino and 'hdr->seq' in ino)
check("Duplicate telemetry logs with serial print",
      'duplicate telemetry' in ino.lower())
check("Duplicate heartbeat logs with serial print",
      'duplicate heartbeat' in ino.lower())
check("lastTelemetrySeq updated on new telemetry",
      'lastTelemetrySeq = hdr->seq' in ino)
check("lastHeartbeatSeq updated on new heartbeat",
      'lastHeartbeatSeq = hdr->seq' in ino)
check("Duplicate telemetry skips handleTelemetry",
      re.search(r'lastTelemetrySeq\).*?\{.*?duplicate.*?\}.*?else.*?handleTelemetry', ino, re.DOTALL) is not None)

# =====================================================================
#  config.h: validateConfig method
# =====================================================================
print("\n== Phase 34: Config validation ==")

with open(os.path.join(GW, 'config.h'), 'r', encoding='utf-8') as f:
    config_h = f.read()

check("validateConfig method exists",
      'bool validateConfig()' in config_h)
check("Validates relayCount range",
      'env.relayCount' in config_h and 'MAX_OUTPUTS' in config_h and 'validateConfig' in config_h)
check("Validates equipment outputIndex",
      'eq.outputIndex' in config_h and '0xFF' in config_h and 'env.relayCount' in config_h)
check("Validates interlockGroup <= 8",
      'interlockGroup > 8' in config_h)
check("Validates sensor scaleX100 range [10, 1000]",
      'scaleX100 < 10' in config_h or 'scaleX100' in config_h and '1000' in config_h)
check("Validates sensor pin <= 69",
      'pin > 69' in config_h or ('s.pin' in config_h and '69' in config_h))
check("Validates rule thresholdX10 range",
      'thresholdX10 < -500' in config_h or ('thresholdX10' in config_h and '1500' in config_h))
check("Validates rule hysteresisX10 range",
      'hysteresisX10 < 0' in config_h or ('hysteresisX10' in config_h and '200' in config_h))
check("Validates cooldownS and durationS <= 86400",
      'cooldownS > 86400' in config_h and 'durationS > 86400' in config_h)
check("Auto-saves on correction",
      'saveEnvironment' in config_h and 'saveEquipment' in config_h and 'saveSensors' in config_h and 'saveRules' in config_h)
check("Returns false when corrections made",
      'return !corrected' in config_h)

# =====================================================================
#  gateway-esp32.ino: validateConfig called in setup
# =====================================================================
print("\n== Phase 34: validateConfig called in setup ==")

check("validateConfig called after config.begin()",
      'config.validateConfig()' in ino)
validate_block = ino[ino.find('config.validateConfig()'):ino.find('config.validateConfig()')+300] if 'config.validateConfig()' in ino else ''
check("Correction logged to eventLog",
      'eventLog.add' in validate_block and 'config' in validate_block.lower())

# Simpler check for eventLog near validateConfig
validate_idx = ino.find('config.validateConfig()')
if validate_idx != -1:
    nearby = ino[validate_idx:validate_idx+300]
    check("eventLog.add called near validateConfig",
          'eventLog.add' in nearby)
else:
    check("eventLog.add called near validateConfig", False)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 34 results: {passed} passed, {failed} failed")
if failed:
    print("SOME TESTS FAILED")
    sys.exit(1)
else:
    print("ALL TESTS PASSED")
