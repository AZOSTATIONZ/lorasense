"""
LoRaSense Protocol Tests
Tests CRC-16/CCITT-FALSE, frame building, and history ring buffer logic.
Run: python tests/test_protocol.py
"""
import struct
import sys

passed = 0
failed = 0

def check(name, condition):
    global passed, failed
    if condition:
        passed += 1
        print(f"  PASS: {name}")
    else:
        failed += 1
        print(f"  FAIL: {name}")


# =====================================================================
#  CRC-16/CCITT-FALSE — must match the C implementation exactly
# =====================================================================
def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc

print("=== CRC-16/CCITT-FALSE ===")
# Known test vectors from the CRC catalog
check("empty data", crc16(b"") == 0xFFFF)
check("ASCII '123456789'", crc16(b"123456789") == 0x29B1)
check("single zero byte", crc16(b"\x00") == 0xE1F0)
check("single 0xFF byte", crc16(b"\xFF") == 0xFF00)
check("two bytes 0x00 0x00", crc16(b"\x00\x00") == 0x1D0F)

# Verify CRC detects bit flips
original = b"\x01\x02\x03\x04\x05"
flipped  = b"\x01\x02\x03\x04\x04"  # last byte changed
check("bit flip detected", crc16(original) != crc16(flipped))


# =====================================================================
#  Frame building and validation
# =====================================================================
PROTOCOL_VERSION = 2
HEADER_SIZE = 5
CRC_SIZE = 2

def build_frame(dst, src, msg_type, seq, payload=b""):
    header = struct.pack("BBBBB", dst, src, PROTOCOL_VERSION, msg_type, seq)
    body = header + payload
    crc = crc16(body)
    return body + struct.pack(">H", crc)

def validate_frame(frame: bytes) -> bool:
    if len(frame) < HEADER_SIZE + CRC_SIZE:
        return False
    received_crc = struct.unpack(">H", frame[-2:])[0]
    computed_crc = crc16(frame[:-2])
    return received_crc == computed_crc

print("\n=== Frame Building ===")

# Build a CMD_SET frame
MSG_CMD_SET = 0x10
ADDR_GATEWAY = 0x01
ADDR_NODE1 = 0x02
cmd_payload = struct.pack("BB", 0, 1)  # device 0, on
frame = build_frame(ADDR_NODE1, ADDR_GATEWAY, MSG_CMD_SET, 42, cmd_payload)

check("frame length correct", len(frame) == HEADER_SIZE + 2 + CRC_SIZE)
check("frame dst byte", frame[0] == ADDR_NODE1)
check("frame src byte", frame[1] == ADDR_GATEWAY)
check("frame version byte", frame[2] == PROTOCOL_VERSION)
check("frame type byte", frame[3] == MSG_CMD_SET)
check("frame seq byte", frame[4] == 42)
check("frame validates", validate_frame(frame))

# Corrupt one byte — should fail validation
corrupted = bytearray(frame)
corrupted[3] ^= 0x01
check("corrupted frame rejected", not validate_frame(bytes(corrupted)))

# Empty payload frame (keepalive)
MSG_KEEPALIVE = 0x50
keepalive = build_frame(ADDR_NODE1, ADDR_GATEWAY, MSG_KEEPALIVE, 0)
check("keepalive length", len(keepalive) == HEADER_SIZE + CRC_SIZE)
check("keepalive validates", validate_frame(keepalive))

# Telemetry frame
MSG_TELEMETRY = 0x30
telem_payload = struct.pack("<hhB", 245, 655, 0b0101)
telem_frame = build_frame(ADDR_GATEWAY, ADDR_NODE1, MSG_TELEMETRY, 7, telem_payload)
check("telemetry validates", validate_frame(telem_frame))

# Parse telemetry payload back
telem_body = telem_frame[HEADER_SIZE:-CRC_SIZE]
temp_x10, hum_x10, relays = struct.unpack("<hhB", telem_body)
check("telemetry temp decode", temp_x10 == 245)
check("telemetry humidity decode", hum_x10 == 655)
check("telemetry relay bits", relays == 0b0101)

# Too-short frame rejected
check("short frame rejected", not validate_frame(b"\x01\x02\x03"))

print("\n=== Frame Roundtrip ===")
# Build then validate many different message types
msg_types = [0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x61, 0xE0, 0xE1]
for mt in msg_types:
    f = build_frame(ADDR_NODE1, ADDR_GATEWAY, mt, 100, b"\xAA\xBB")
    check(f"msg type 0x{mt:02X} roundtrip", validate_frame(f))


# =====================================================================
#  History ring buffer logic
# =====================================================================
print("\n=== History Ring Buffer ===")

class SensorHistory:
    def __init__(self, max_size=288):
        self.max_size = max_size
        self.buf = [None] * max_size
        self.head = 0
        self.count = 0

    def push(self, uptime_sec, temp, hum):
        self.buf[self.head] = {
            "t": uptime_sec,
            "tc": round(temp, 1),
            "h": round(hum, 1),
        }
        self.head = (self.head + 1) % self.max_size
        if self.count < self.max_size:
            self.count += 1

    def oldest(self):
        if self.count < self.max_size:
            return 0
        return self.head

    def at(self, i):
        idx = (self.oldest() + i) % self.max_size
        return self.buf[idx]

# Basic push and read
h = SensorHistory(max_size=5)
h.push(100, 24.5, 60.0)
h.push(400, 25.0, 58.0)
check("count after 2 pushes", h.count == 2)
check("first reading time", h.at(0)["t"] == 100)
check("first reading temp", h.at(0)["tc"] == 24.5)
check("second reading hum", h.at(1)["h"] == 58.0)

# Fill to capacity
h.push(700, 25.5, 55.0)
h.push(1000, 26.0, 52.0)
h.push(1300, 26.5, 50.0)
check("count at capacity", h.count == 5)
check("oldest is index 0 when full", h.oldest() == 0)
check("readings in order", h.at(0)["t"] == 100)
check("last reading", h.at(4)["t"] == 1300)

# Wrap around — push one more, oldest should be evicted
h.push(1600, 27.0, 48.0)
check("count stays at max", h.count == 5)
check("oldest reading evicted", h.at(0)["t"] == 400)
check("newest reading", h.at(4)["t"] == 1600)

# Push several more to test continued wrapping
h.push(1900, 27.5, 46.0)
h.push(2200, 28.0, 44.0)
check("after multiple wraps", h.at(0)["t"] == 1000)
check("newest after wraps", h.at(4)["t"] == 2200)

# Verify ordering is always chronological
times = [h.at(i)["t"] for i in range(h.count)]
check("chronological order maintained", times == sorted(times))

# Edge case: single-element buffer
h1 = SensorHistory(max_size=1)
h1.push(10, 20.0, 50.0)
check("single-slot buffer works", h1.at(0)["t"] == 10)
h1.push(20, 21.0, 51.0)
check("single-slot overwrite", h1.at(0)["t"] == 20)
check("single-slot count stays 1", h1.count == 1)


# =====================================================================
#  Automation rule hysteresis logic
# =====================================================================
print("\n=== Automation Hysteresis ===")

def evaluate_rule(value_x10, threshold_x10, hysteresis_x10, condition, currently_triggered):
    """Mirrors the C evaluateRules() logic."""
    if condition == 0:  # "above"
        if currently_triggered:
            return value_x10 >= (threshold_x10 - hysteresis_x10)
        else:
            return value_x10 > threshold_x10
    else:  # "below"
        if currently_triggered:
            return value_x10 <= (threshold_x10 + hysteresis_x10)
        else:
            return value_x10 < threshold_x10

# "Above 30°C with 2°C hysteresis" — fan control scenario
check("above: 29°C doesn't trigger", not evaluate_rule(290, 300, 20, 0, False))
check("above: 30°C doesn't trigger (not >)", not evaluate_rule(300, 300, 20, 0, False))
check("above: 31°C triggers", evaluate_rule(310, 300, 20, 0, False))
check("above: 29°C stays triggered (hysteresis)", evaluate_rule(290, 300, 20, 0, True))
check("above: 28°C stays triggered (at boundary)", evaluate_rule(280, 300, 20, 0, True))
check("above: 27.9°C releases", not evaluate_rule(279, 300, 20, 0, True))

# "Below 18°C with 1°C hysteresis" — heating scenario
check("below: 19°C doesn't trigger", not evaluate_rule(190, 180, 10, 1, False))
check("below: 18°C doesn't trigger (not <)", not evaluate_rule(180, 180, 10, 1, False))
check("below: 17°C triggers", evaluate_rule(170, 180, 10, 1, False))
check("below: 18.5°C stays triggered (hysteresis)", evaluate_rule(185, 180, 10, 1, True))
check("below: 19°C stays triggered (at boundary)", evaluate_rule(190, 180, 10, 1, True))
check("below: 19.1°C releases", not evaluate_rule(191, 180, 10, 1, True))


# =====================================================================
#  Summary
# =====================================================================
print(f"\n{'='*40}")
print(f"  {passed} passed, {failed} failed")
print(f"{'='*40}")
sys.exit(1 if failed else 0)
