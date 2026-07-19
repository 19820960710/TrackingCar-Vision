import unittest

import cv2
import numpy as np

from app.models import new_measurement
from vision.laser.detector import LaserDetector


def stable_target():
    target = new_measurement("target")
    target.update({"valid": True, "updated": True, "predicted": False, "rect": (40, 30, 240, 180), "timestamp_ms": 1})
    return target


def frame_with_points(*points):
    frame = np.full((240, 320, 3), 220, dtype=np.uint8)
    for x, y in points:
        cv2.circle(frame, (x, y), 4, (255, 0, 128), thickness=-1)
    return frame


class LaserDetectorTest(unittest.TestCase):
    def _detector(self):
        return LaserDetector(background_required_frames=2, min_area=2, max_area=100, confirm_frames=2)

    def test_laser_requires_two_frames_and_ignores_calibrated_background(self):
        detector = self._detector()
        target = stable_target()
        background = frame_with_points((70, 60))
        detector.detect(background, {"timestamp_ms": 1, "laser_calibration": True}, target)
        detector.detect(background, {"timestamp_ms": 2, "laser_calibration": True}, target)
        first = detector.detect(frame_with_points((70, 60), (160, 120)), {"timestamp_ms": 3}, target)
        second = detector.detect(frame_with_points((70, 60), (160, 120)), {"timestamp_ms": 4}, target)
        self.assertFalse(first["valid"])
        self.assertTrue(second["valid"], second)
        self.assertAlmostEqual(second["x"], 160, delta=2)
        self.assertAlmostEqual(second["y"], 120, delta=2)

    def test_disappearance_returns_invalid_not_old_laser_point(self):
        detector = self._detector()
        target = stable_target()
        for timestamp in (1, 2):
            detector.detect(frame_with_points((160, 120)), {"timestamp_ms": timestamp}, target)
        missing = detector.detect(frame_with_points(), {"timestamp_ms": 3}, target)
        self.assertFalse(missing["valid"])
        self.assertEqual((missing["x"], missing["y"]), (0, 0))
        self.assertEqual(missing["corners"], None)

    def test_large_jump_requires_new_two_frame_confirmation(self):
        detector = self._detector()
        target = stable_target()
        for timestamp in (1, 2):
            detector.detect(frame_with_points((100, 100)), {"timestamp_ms": timestamp}, target)
        first_jump = detector.detect(frame_with_points((220, 150)), {"timestamp_ms": 3}, target)
        second_jump = detector.detect(frame_with_points((220, 150)), {"timestamp_ms": 4}, target)
        self.assertFalse(first_jump["valid"])
        self.assertTrue(second_jump["valid"], second_jump)
        self.assertEqual(second_jump["reason"], "LASER_RELOCKED")

    def test_predicted_target_never_enables_laser(self):
        detector = self._detector()
        target = stable_target()
        target["updated"] = False
        target["predicted"] = True
        result = detector.detect(frame_with_points((160, 120)), {"timestamp_ms": 1}, target)
        self.assertFalse(result["valid"])
        self.assertEqual(result["reason"], "TARGET_NOT_STABLE")


if __name__ == "__main__":
    unittest.main()
