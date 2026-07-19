"""Executable reference for the MSPM0 binary-frame safety policy.

This is a Python reference and test oracle, not MSPM0 firmware.  Port the
same byte parsing and decision table to the MCU before enabling binary mode.
"""

from protocol.binary_v1 import decode


def is_newer_sequence(new_sequence, previous_sequence):
    if previous_sequence is None:
        return True
    delta = (int(new_sequence) - int(previous_sequence)) & 0xFFFF
    return 0 < delta < 0x8000


class Mspm0SafetyReference:
    def __init__(self, watchdog_ms=100, allow_predicted=False):
        self.watchdog_ms = int(watchdog_ms)
        self.allow_predicted = bool(allow_predicted)
        self.last_sequence = None
        self.last_accepted_ms = None

    def _result(self, action, control_update, parsed=None):
        return {"action": action, "control_update": bool(control_update), "frame": parsed}

    def ingest(self, raw_frame, now_ms):
        parsed = decode(raw_frame)
        if not parsed.get("valid"):
            return self._result("DROP_%s" % parsed["reason"], False, parsed)
        if not is_newer_sequence(parsed["sequence"], self.last_sequence):
            return self._result("DROP_OLD", False, parsed)
        self.last_sequence = parsed["sequence"]
        if parsed["lost"] or parsed["stale"] or not parsed["target_valid"]:
            return self._result("STOP_LOST", False, parsed)
        if parsed["age_ms"] > self.watchdog_ms:
            return self._result("STOP_STALE", False, parsed)
        self.last_accepted_ms = int(now_ms)
        if parsed["predicted"]:
            return self._result("FOLLOW_PREDICTED" if self.allow_predicted else "HOLD_PREDICTED", self.allow_predicted, parsed)
        if not parsed["laser_valid"]:
            return self._result("HOLD_NO_LASER", False, parsed)
        return self._result("FOLLOW", True, parsed)

    def watchdog(self, now_ms):
        if self.last_accepted_ms is None or int(now_ms) - self.last_accepted_ms > self.watchdog_ms:
            return self._result("STOP_WATCHDOG", False)
        return self._result("WATCHDOG_OK", False)
