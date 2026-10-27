#!/usr/bin/env python3
"""
Phase 26 tests: multi-node support.
Verifies protocol address constants, gateway multi-node state management,
per-node config sync, node firmware configurable address, and dashboard updates.
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

FW = os.path.join(os.path.dirname(__file__), '..', 'firmware')
GW = os.path.join(FW, 'gateway-esp32')
NODE = os.path.join(FW, 'node-mega')
COMMON = os.path.join(FW, 'common')
DATA = os.path.join(GW, 'data')

# =====================================================================
#  Protocol: multi-node address constants
# =====================================================================
print("\n== Protocol: address constants ==")

for label, path in [("common/protocol.h", os.path.join(COMMON, 'protocol.h')),
                     ("node-mega/protocol.h", os.path.join(NODE, 'protocol.h'))]:
    with open(path, 'r', encoding='utf-8') as f:
        src = f.read()
    check(f"{label} has ADDR_GATEWAY", "ADDR_GATEWAY" in src and "0x01" in src)
    check(f"{label} has ADDR_NODE1", "ADDR_NODE1" in src and "0x02" in src)
    check(f"{label} has ADDR_NODE2", "ADDR_NODE2" in src and "0x03" in src)
    check(f"{label} has ADDR_NODE3", "ADDR_NODE3" in src and "0x04" in src)
    check(f"{label} has ADDR_NODE4", "ADDR_NODE4" in src and "0x05" in src)
    check(f"{label} has ADDR_BROADCAST", "ADDR_BROADCAST" in src and "0xFF" in src)
    check(f"{label} has MAX_NODES_ADDR", "MAX_NODES_ADDR" in src)

# =====================================================================
#  Gateway: device.h NodeState struct
# =====================================================================
print("\n== Gateway: device.h NodeState ==")

with open(os.path.join(GW, 'device.h'), 'r', encoding='utf-8') as f:
    dh = f.read()

check("NodeState struct defined", "struct NodeState" in dh)
check("MAX_NODE_SLOTS defined", "MAX_NODE_SLOTS" in dh)
check("NodeState has loraAddress", "loraAddress" in dh)
check("NodeState has online field", "bool     online" in dh or "bool online" in dh)
check("NodeState has temperature", "temperature" in dh)
check("NodeState has humidity", "humidity" in dh)
check("NodeState has analogValue", "analogValue" in dh)
check("NodeState has configSynced", "configSynced" in dh)
check("NodeState has lastConfigPush", "lastConfigPush" in dh)
check("NodeState has rssi", "rssi" in dh)
check("NodeState has reset()", "void reset()" in dh)

# =====================================================================
#  Gateway: multi-node state management
# =====================================================================
print("\n== Gateway: multi-node state management ==")

with open(os.path.join(GW, 'gateway-esp32.ino'), 'r', encoding='utf-8') as f:
    gw = f.read()

check("nodeStates array declared", "NodeState nodeStates[MAX_NODE_SLOTS]" in gw)
check("findNodeByAddress helper", "NodeState *findNodeByAddress" in gw)
check("findNodeById helper", "NodeState *findNodeById" in gw)
check("primaryNode helper", "NodeState *primaryNode" in gw)
check("anyNodeOnline helper", "bool anyNodeOnline" in gw)
check("allNodesSynced helper", "bool allNodesSynced" in gw)

# No old single-node globals
check("no old nodeOnline global", "nodeOnline" not in gw)
check("no old nodeTemperature global", "nodeTemperature" not in gw)
check("no old nodeHumidity global", "nodeHumidity" not in gw)
check("no old nodeAnalogValue global", "nodeAnalogValue" not in gw)
check("no old lastNodeHeard global", "lastNodeHeard" not in gw)

# =====================================================================
#  Gateway: per-node config sync
# =====================================================================
print("\n== Gateway: per-node config sync ==")

check("pushConfigToNode takes NodeInstance", "void pushConfigToNode(NodeInstance &node)" in gw)
check("bumpConfigVersion marks all unsynced", "nodeStates[i].configSynced = false" in gw)
check("config push checks per-node sync", "ns->configSynced" in gw or "!ns->configSynced" in gw)

# =====================================================================
#  Gateway: per-node handleState
# =====================================================================
print("\n== Gateway: per-node handleState ==")

check("handleState uses anyNodeOnline", 'anyNodeOnline()' in gw)
check("handleState uses allNodesSynced", 'allNodesSynced()' in gw)
check("handleState uses primaryNode", 'primaryNode()' in gw)

# =====================================================================
#  Gateway: per-node evaluateRules
# =====================================================================
print("\n== Gateway: per-node evaluateRules ==")

check("evaluateRules uses findNodeById", "findNodeById(eq->nodeId)" in gw)
check("evaluateRules reads rns->temperature", "rns->temperature" in gw)
check("evaluateRules reads rns->humidity", "rns->humidity" in gw)
check("evaluateRules reads rns->analogValue", "rns->analogValue" in gw)

# =====================================================================
#  Gateway: sendCommand routes to correct node
# =====================================================================
print("\n== Gateway: sendCommand routing ==")

check("sendCommand resolves equipment owner", "config.findByOutput(outputIdx)" in gw)
check("sendCommand looks up node address", "hw.findNode(owner->nodeId)" in gw or "ni->address" in gw)

# =====================================================================
#  Gateway: setup initializes nodeStates
# =====================================================================
print("\n== Gateway: setup nodeState init ==")

check("setup resets nodeStates", "nodeStates[i].reset()" in gw)
check("setup assigns loraAddress", "loraAddress = hw.nodes[i].address" in gw)

# =====================================================================
#  Gateway: loop per-node timeout
# =====================================================================
print("\n== Gateway: loop per-node timeout ==")

check("loop checks per-node timeout", "ns->online && millis() - ns->lastHeard > NODE_TIMEOUT_MS" in gw
      or "ns->lastHeard" in gw)

# =====================================================================
#  Node firmware: configurable address
# =====================================================================
print("\n== Node: configurable address ==")

with open(os.path.join(NODE, 'node-mega.ino'), 'r', encoding='utf-8') as f:
    node = f.read()

check("MY_ADDRESS constant defined", "const uint8_t MY_ADDRESS" in node)
check("MY_ADDRESS defaults to ADDR_NODE1", "MY_ADDRESS = ADDR_NODE1" in node)
check("buildFrame uses MY_ADDRESS (not ADDR_NODE1)", node.count("MY_ADDRESS") >= 6)
check("no hardcoded ADDR_NODE1 in buildFrame", "buildFrame(buf, ADDR_GATEWAY, ADDR_NODE1" not in node)
check("dst check uses MY_ADDRESS", "hdr->dst == MY_ADDRESS" in node)

# =====================================================================
#  Dashboard: per-node RSSI in Hardware Studio
# =====================================================================
print("\n== Dashboard: per-node display ==")

with open(os.path.join(DATA, 'js', 'app.js'), 'r', encoding='utf-8') as f:
    js = f.read()

check("node card shows RSSI", "n.rssi" in js)
check("node card shows online status", "n.online" in js)
check("dashboard still uses node_online from state", "s.node_online" in js)

# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*60}")
print(f"Phase 26 results: {passed} passed, {failed} failed")
if failed:
    print("SOME TESTS FAILED")
    sys.exit(1)
else:
    print("ALL TESTS PASSED")
