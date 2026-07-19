import unittest

from protocol.binary_v1 import FRAME_SIZE, STATUS_TARGET_PREDICTED, decode, encode
from protocol.mspm0_safety_reference import Mspm0SafetyReference


def observation(predicted=False, target_valid=True, age_ms=0, laser_valid=True):
    return {
        "target": {"valid": target_valid, "x": 244, "y": 168, "age_ms": age_ms, "predicted": predicted},
        "laser": {"valid": laser_valid, "x": 256, "y": 160, "age_ms": 0},
    }


class BinaryProtocolTest(unittest.TestCase):
    def test_normal_frame_round_trip_and_follow(self):
        frame = encode(observation(), 7)
        self.assertEqual(len(frame), FRAME_SIZE)
        parsed = decode(frame)
        self.assertTrue(parsed["valid"])
        self.assertEqual((parsed["dx"], parsed["dy"]), (-12, 8))
        decision = Mspm0SafetyReference().ingest(frame, 10)
        self.assertEqual(decision["action"], "FOLLOW")
        self.assertTrue(decision["control_update"])

    def test_predicted_frame_has_status_and_is_held_by_default(self):
        frame = encode(observation(predicted=True, age_ms=30), 8)
        parsed = decode(frame)
        self.assertTrue(parsed["status"] & STATUS_TARGET_PREDICTED)
        self.assertTrue(parsed["predicted"])
        decision = Mspm0SafetyReference().ingest(frame, 20)
        self.assertEqual(decision["action"], "HOLD_PREDICTED")
        self.assertFalse(decision["control_update"])

    def test_lost_and_stale_frames_stop(self):
        lost = encode(observation(target_valid=False), 9)
        stale = encode(observation(age_ms=101), 10)
        self.assertEqual(Mspm0SafetyReference().ingest(lost, 30)["action"], "STOP_LOST")
        self.assertEqual(Mspm0SafetyReference().ingest(stale, 40)["action"], "STOP_LOST")

    def test_old_frame_and_crc_error_do_not_refresh_watchdog(self):
        receiver = Mspm0SafetyReference()
        normal = encode(observation(), 100)
        self.assertEqual(receiver.ingest(normal, 0)["action"], "FOLLOW")
        old = encode(observation(), 99)
        self.assertEqual(receiver.ingest(old, 10)["action"], "DROP_OLD")
        damaged = bytearray(encode(observation(), 101))
        damaged[8] ^= 0x01
        self.assertEqual(receiver.ingest(damaged, 20)["action"], "DROP_CRC")
        self.assertEqual(receiver.watchdog(101)["action"], "STOP_WATCHDOG")


if __name__ == "__main__":
    unittest.main()
