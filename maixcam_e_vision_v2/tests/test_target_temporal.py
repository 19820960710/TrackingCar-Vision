import unittest

from app.models import new_measurement
from settings import (
    TARGET_KALMAN_MEASUREMENT_NOISE, TARGET_KALMAN_PROCESS_NOISE,
    TARGET_LOWPASS_ALPHA, TARGET_OUTLIER_JUMP_MIN_PX, TARGET_OUTLIER_JUMP_RATIO,
)
from vision.filters.target_temporal import TargetTemporalProcessor


def raw_target(x, y, timestamp_ms):
    result = new_measurement("target")
    result.update({
        "valid": True, "updated": True, "x": x, "y": y,
        "rect": (x - 20, y - 20, 40, 40),
        "corners": ((x - 20, y - 20), (x + 20, y - 20), (x + 20, y + 20), (x - 20, y + 20)),
        "confidence": 90, "timestamp_ms": timestamp_ms, "reason": "OK", "mode": "TRADITIONAL_ROI",
    })
    return result


class TargetTemporalTest(unittest.TestCase):
    def test_initial_capture_requires_three_current_frames(self):
        processor = TargetTemporalProcessor(use_kalman=False)
        self.assertFalse(processor.update(raw_target(100, 100, 0))["valid"])
        self.assertFalse(processor.update(raw_target(101, 100, 16))["valid"])
        result = processor.update(raw_target(102, 100, 32))
        self.assertTrue(result["valid"])
        self.assertTrue(result["updated"])
        self.assertFalse(result["predicted"])
        self.assertEqual(result["age_ms"], 0)

    def test_outlier_and_short_occlusion_predict_without_jump(self):
        processor = TargetTemporalProcessor(use_kalman=False)
        for timestamp in (0, 16, 32):
            stable = processor.update(raw_target(100, 100, timestamp))
        outlier = processor.update(raw_target(260, 260, 48))
        self.assertTrue(outlier["valid"])
        self.assertTrue(outlier["predicted"])
        self.assertFalse(outlier["updated"])
        self.assertLess(abs(outlier["x"] - stable["x"]), 1)
        hidden = processor.update(new_measurement("target"), 64)
        self.assertTrue(hidden["predicted"])
        self.assertEqual(hidden["age_ms"], 32)

    def test_production_filter_tracks_constant_motion_without_large_lag(self):
        processor = TargetTemporalProcessor(
            acquire_frames=1, lowpass_alpha=TARGET_LOWPASS_ALPHA,
            jump_min_px=TARGET_OUTLIER_JUMP_MIN_PX,
            jump_ratio=TARGET_OUTLIER_JUMP_RATIO, use_kalman=True,
            kalman_process_noise=TARGET_KALMAN_PROCESS_NOISE,
            kalman_measurement_noise=TARGET_KALMAN_MEASUREMENT_NOISE,
        )
        for frame_index in range(30):
            raw_x = 100 + frame_index * 10
            result = processor.update(raw_target(raw_x, 100, frame_index * 33))

        self.assertTrue(result["valid"])
        self.assertLess(raw_x - result["x"], 8.0)

    def test_fast_consistent_motion_uses_last_raw_point_not_filtered_lag(self):
        processor = TargetTemporalProcessor(
            use_kalman=True, lowpass_alpha=0.45,
            jump_min_px=28, jump_ratio=0.50,
        )
        for x, timestamp in ((100, 0), (108, 33), (116, 66)):
            result = processor.update(raw_target(x, 100, timestamp))
        self.assertTrue(result["valid"])

        moved = processor.update(raw_target(140, 100, 99))

        self.assertTrue(moved["valid"])
        self.assertTrue(moved["updated"])
        self.assertFalse(moved["predicted"])

    def test_corrupt_prediction_state_fails_safe_instead_of_crashing(self):
        processor = TargetTemporalProcessor(use_kalman=False)
        processor.last_real = raw_target(100, 100, 0)
        processor.filtered_point = None

        result = processor.update(new_measurement("target"), 16)

        self.assertFalse(result["valid"])
        self.assertIn("INTERNAL_STATE", result["reason"])

    def test_prediction_expiry_never_keeps_old_control_point(self):
        processor = TargetTemporalProcessor(use_kalman=False, predict_hold_ms=80, predict_max_frames=5)
        for timestamp in (0, 16, 32):
            processor.update(raw_target(100, 100, timestamp))
        for timestamp in (42, 52, 62, 72, 82):
            self.assertTrue(processor.update(new_measurement("target"), timestamp)["valid"])
        expired = processor.update(new_measurement("target"), 92)
        self.assertFalse(expired["valid"])
        self.assertFalse(expired["predicted"])
        self.assertEqual((expired["x"], expired["y"]), (0, 0))
        self.assertIn("PREDICTION_EXPIRED", expired["reason"])

    def test_reacquisition_requires_two_frames_after_loss(self):
        processor = TargetTemporalProcessor(use_kalman=False, predict_hold_ms=20)
        for timestamp in (0, 16, 32):
            processor.update(raw_target(100, 100, timestamp))
        self.assertFalse(processor.update(new_measurement("target"), 60)["valid"])
        first = processor.update(raw_target(104, 100, 76))
        second = processor.update(raw_target(105, 100, 92))
        self.assertFalse(first["valid"])
        self.assertTrue(second["valid"])
        self.assertTrue(second["updated"])


if __name__ == "__main__":
    unittest.main()
