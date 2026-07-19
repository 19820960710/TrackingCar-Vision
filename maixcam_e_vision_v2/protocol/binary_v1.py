"""Binary protocol V1, disabled by default until MSPM0 firmware is upgraded."""

import struct

from protocol.crc16 import crc16_ibm

HEADER = b"\xA5\x5A"
VERSION = 1
PAYLOAD_FORMAT = "<BHBHhhhhhh"
FRAME_FORMAT = "<2sBHBHhhhhhhH"
FRAME_SIZE = struct.calcsize(FRAME_FORMAT)

STATUS_TARGET_VALID = 0x01
STATUS_LASER_VALID = 0x02
STATUS_TARGET_PREDICTED = 0x04
STATUS_TARGET_LOST = 0x08
STATUS_TARGET_STALE = 0x10


def _i16(value):
    return max(-32768, min(32767, int(round(value))))


def _age(measurement):
    return max(0, min(65535, int((measurement or {}).get("age_ms", 0))))


def _target_status(target, max_age_ms):
    target = target or {}
    age_ms = _age(target)
    status = 0
    if not target.get("valid") or target.get("lost_hold"):
        return status | STATUS_TARGET_LOST, age_ms
    if age_ms > max_age_ms:
        return status | STATUS_TARGET_LOST | STATUS_TARGET_STALE, age_ms
    status |= STATUS_TARGET_VALID
    if target.get("predicted"):
        status |= STATUS_TARGET_PREDICTED
    return status, age_ms


def encode(observation, sequence, max_age_ms=100):
    """Encode one V1 frame. Calling this does not enable binary transport."""
    observation = observation or {}
    target = observation.get("target") or {}
    laser = observation.get("laser") or {}
    status, age_ms = _target_status(target, max_age_ms)
    target_ok = bool(status & STATUS_TARGET_VALID)
    laser_ok = bool(laser.get("valid")) and not laser.get("lost_hold") and _age(laser) <= max_age_ms
    if laser_ok:
        status |= STATUS_LASER_VALID
    target_x = _i16(target.get("x", 0)) if target_ok else 0
    target_y = _i16(target.get("y", 0)) if target_ok else 0
    laser_x = _i16(laser.get("x", 0)) if laser_ok else 0
    laser_y = _i16(laser.get("y", 0)) if laser_ok else 0
    dx = _i16(target_x - laser_x) if target_ok and laser_ok else 0
    dy = _i16(target_y - laser_y) if target_ok and laser_ok else 0
    payload = struct.pack(PAYLOAD_FORMAT, VERSION, int(sequence) & 0xFFFF, status, age_ms, target_x, target_y, laser_x, laser_y, dx, dy)
    raw = HEADER + payload
    return raw + struct.pack("<H", crc16_ibm(raw))


def decode(frame):
    """Return parsed frame or an explicit error dictionary; never raise on UART bytes."""
    if not isinstance(frame, (bytes, bytearray)) or len(frame) != FRAME_SIZE:
        return {"valid": False, "reason": "FRAME_LENGTH"}
    raw = bytes(frame)
    try:
        header, version, sequence, status, age_ms, target_x, target_y, laser_x, laser_y, dx, dy, received_crc = struct.unpack(FRAME_FORMAT, raw)
    except struct.error:
        return {"valid": False, "reason": "FRAME_FORMAT"}
    if header != HEADER:
        return {"valid": False, "reason": "FRAME_HEADER"}
    if version != VERSION:
        return {"valid": False, "reason": "FRAME_VERSION"}
    if crc16_ibm(raw[:-2]) != received_crc:
        return {"valid": False, "reason": "CRC"}
    return {
        "valid": True, "version": version, "sequence": sequence, "status": status,
        "age_ms": age_ms, "target_x": target_x, "target_y": target_y,
        "laser_x": laser_x, "laser_y": laser_y, "dx": dx, "dy": dy,
        "target_valid": bool(status & STATUS_TARGET_VALID),
        "laser_valid": bool(status & STATUS_LASER_VALID),
        "predicted": bool(status & STATUS_TARGET_PREDICTED),
        "lost": bool(status & STATUS_TARGET_LOST),
        "stale": bool(status & STATUS_TARGET_STALE),
    }
