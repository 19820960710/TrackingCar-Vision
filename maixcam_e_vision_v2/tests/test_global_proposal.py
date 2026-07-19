import unittest
from unittest import mock

import cv2
import numpy as np

from vision.target.detector import TargetDetector
from vision.target.global_proposal import propose_black_frame


class GlobalProposalTest(unittest.TestCase):
    def test_black_rectangle_yields_coarse_roi(self):
        frame = np.full((240, 320, 3), 220, dtype=np.uint8)
        cv2.rectangle(frame, (70, 40), (250, 200), (0, 0, 0), thickness=12)
        result = propose_black_frame(frame, timestamp_ms=70)
        self.assertTrue(result["valid"], result)
        self.assertEqual(result["timestamp_ms"], 70)
        self.assertGreater(result["confidence"], 0)
        x, y, width, height = result["rect"]
        self.assertAlmostEqual(x + width / 2, 160, delta=15)
        self.assertAlmostEqual(y + height / 2, 120, delta=15)

    def test_global_black_path_skips_gray_and_gaussian_conversion(self):
        frame = np.full((240, 320, 3), 220, dtype=np.uint8)
        cv2.rectangle(frame, (145, 100), (175, 140), (0, 0, 0), thickness=3)
        with mock.patch.object(cv2, "cvtColor", side_effect=AssertionError("global path must use BGR")), \
                mock.patch.object(cv2, "GaussianBlur", side_effect=AssertionError("global path must not blur")):
            result = propose_black_frame(frame, timestamp_ms=72)

        self.assertTrue(result["valid"], result)

    def test_colored_dark_shape_is_not_treated_as_black_frame(self):
        frame = np.full((240, 320, 3), 220, dtype=np.uint8)
        cv2.rectangle(frame, (120, 70), (200, 170), (180, 25, 25), thickness=8)

        result = propose_black_frame(frame, timestamp_ms=73)

        self.assertFalse(result["valid"], result)
    def test_small_distant_frame_is_acquired_after_normal_confirmation(self):
        frame = np.full((240, 320, 3), 220, dtype=np.uint8)
        cv2.rectangle(frame, (152, 108), (168, 132), (0, 0, 0), thickness=2)
        detector = TargetDetector()

        results = [
            detector.detect(frame, {"timestamp_ms": timestamp})
            for timestamp in (100, 133, 166)
        ]

        self.assertFalse(results[0]["valid"])
        self.assertFalse(results[1]["valid"])
        self.assertTrue(results[2]["valid"], results[2])
        self.assertAlmostEqual(results[2]["x"], 160, delta=5)
        self.assertAlmostEqual(results[2]["y"], 120, delta=5)

    def test_extreme_wide_dark_shape_is_rejected_by_aspect_gate(self):
        frame = np.full((240, 320, 3), 220, dtype=np.uint8)
        cv2.rectangle(frame, (90, 112), (230, 128), (0, 0, 0), thickness=-1)

        result = propose_black_frame(frame, timestamp_ms=75)

        self.assertFalse(result["valid"], result)
    def test_blank_frame_is_explicitly_invalid(self):
        frame = np.full((240, 320, 3), 220, dtype=np.uint8)
        result = propose_black_frame(frame, timestamp_ms=80)
        self.assertFalse(result["valid"])
        self.assertIsNone(result["rect"])
        self.assertEqual(result["timestamp_ms"], 80)


if __name__ == "__main__":
    unittest.main()
