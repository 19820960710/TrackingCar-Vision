import unittest
from unittest import mock

import cv2
import numpy as np

from vision.target.roi_refine import _remove_border_connected_white, refine


class RoiRefineTest(unittest.TestCase):
    def _target_frame(self, outer, thickness=14):
        frame = np.full((240, 320, 3), 120, dtype=np.uint8)
        outer = np.array(outer, dtype=np.int32)
        cv2.fillConvexPoly(frame, outer, (245, 245, 245))
        cv2.polylines(frame, [outer], True, (0, 0, 0), thickness=thickness)
        return frame

    def test_static_tilted_target_returns_current_corners_and_center(self):
        frame = self._target_frame([[60, 30], [260, 45], [245, 210], [75, 195]])
        cv2.circle(frame, (160, 120), 4, (0, 0, 255), thickness=-1)
        result = refine(frame, (30, 10, 260, 220), timestamp_ms=123)
        self.assertTrue(result["valid"], result)
        self.assertTrue(result["updated"])
        self.assertFalse(result["predicted"])
        self.assertEqual(result["timestamp_ms"], 123)
        self.assertEqual(len(result["corners"]), 4)
        self.assertAlmostEqual(result["x"], 160, delta=12)
        self.assertAlmostEqual(result["y"], 120, delta=12)

    def test_clear_otsu_fast_path_skips_gaussian_blur(self):
        frame = self._target_frame([[60, 30], [260, 45], [245, 210], [75, 195]])
        with mock.patch.object(cv2, "GaussianBlur", side_effect=AssertionError("fast path must not blur")):
            result = refine(frame, (30, 10, 260, 220), timestamp_ms=190)

        self.assertTrue(result["valid"], result)
    def test_adaptive_fallback_runs_when_fast_threshold_finds_nothing(self):
        frame = self._target_frame([[60, 30], [260, 45], [245, 210], [75, 195]])
        empty_fast_binary = np.zeros((220, 260), dtype=np.uint8)
        with mock.patch.object(cv2, "threshold", return_value=(0, empty_fast_binary)), \
                mock.patch.object(cv2, "medianBlur", side_effect=AssertionError("redundant fallback blur")):
            result = refine(frame, (30, 10, 260, 220), timestamp_ms=200)
        self.assertTrue(result["valid"], result)

    def test_smaller_tilted_target_at_a_second_scale(self):
        result = refine(
            self._target_frame([[95, 55], [225, 65], [215, 180], [105, 170]], thickness=10),
            (50, 25, 220, 190),
            timestamp_ms=234,
        )
        self.assertTrue(result["valid"], result)
        self.assertAlmostEqual(result["x"], 160, delta=10)
        self.assertAlmostEqual(result["y"], 118, delta=10)

    def test_single_flood_fill_keeps_only_enclosed_white_region(self):
        binary = np.full((80, 100), 255, dtype=np.uint8)
        cv2.rectangle(binary, (20, 15), (80, 65), 0, thickness=4)

        enclosed = _remove_border_connected_white(binary, cv2, np)

        self.assertEqual(int(enclosed[0, 0]), 0)
        self.assertEqual(int(enclosed[40, 50]), 255)

    def test_blank_roi_returns_invalid_without_fake_old_coordinates(self):
        frame = np.full((240, 320, 3), 120, dtype=np.uint8)
        result = refine(frame, (30, 10, 260, 220), timestamp_ms=456)
        self.assertFalse(result["valid"])
        self.assertFalse(result["updated"])
        self.assertFalse(result["predicted"])
        self.assertFalse(result["lost_hold"])
        self.assertEqual(result["corners"], None)
        self.assertEqual((result["x"], result["y"]), (0, 0))
        self.assertEqual(result["timestamp_ms"], 456)


if __name__ == "__main__":
    unittest.main()
