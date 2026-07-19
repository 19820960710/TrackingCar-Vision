"""Temporal confirmation, outlier gating, low-pass filtering and prediction."""

import math

from app.models import new_measurement
from vision.filters.kalman import ConstantVelocityKalman
from vision.filters.lowpass import blend
from vision.geometry.quadrilateral import is_convex_quad


class TargetTemporalProcessor:
    """Turn current-frame target measurements into safe temporal measurements."""

    def __init__(self, acquire_frames=3, retrack_frames=2, lowpass_alpha=0.45,
                 jump_min_px=12, jump_ratio=0.30, predict_hold_ms=80,
                 predict_max_frames=5, use_kalman=True,
                 kalman_process_noise=30.0, kalman_measurement_noise=9.0):
        self.acquire_frames = acquire_frames
        self.retrack_frames = retrack_frames
        self.lowpass_alpha = lowpass_alpha
        self.jump_min_px = jump_min_px
        self.jump_ratio = jump_ratio
        self.predict_hold_ms = predict_hold_ms
        self.predict_max_frames = predict_max_frames
        self.use_kalman = use_kalman
        self.kalman = ConstantVelocityKalman(kalman_process_noise, kalman_measurement_noise)
        self.confirmed = False
        self.ever_confirmed = False
        self.valid_streak = 0
        self.predict_frames = 0
        self.last_real = None
        self.filtered_point = None
        self.last_raw_point = None

    def _invalid(self, reason, timestamp_ms):
        result = new_measurement("target")
        age_ms = 0 if self.last_real is None else max(0, int(timestamp_ms) - int(self.last_real["timestamp_ms"]))
        result.update({"timestamp_ms": int(timestamp_ms), "age_ms": age_ms, "reason": reason, "mode": "TEMPORAL"})
        return result

    def _is_legal(self, measurement):
        if not isinstance(measurement, dict) or not measurement.get("valid"):
            return False, "RAW_INVALID"
        try:
            x, y = float(measurement["x"]), float(measurement["y"])
            if not math.isfinite(x) or not math.isfinite(y):
                return False, "NONFINITE_POINT"
            corners = measurement.get("corners")
            if corners is None or not is_convex_quad(corners):
                return False, "ILLEGAL_CORNERS"
        except (KeyError, TypeError, ValueError):
            return False, "ILLEGAL_MEASUREMENT"
        return True, "OK"

    def _jump_limit(self, measurement):
        rect = measurement.get("rect") or (0, 0, 0, 0)
        try:
            scale = max(float(rect[2]), float(rect[3]))
        except (IndexError, TypeError, ValueError):
            scale = 0.0
        return max(float(self.jump_min_px), self.jump_ratio * scale)

    def _is_outlier(self, measurement):
        if self.last_real is None:
            return False
        reference = self.last_raw_point or (self.last_real["x"], self.last_real["y"])
        dx = float(measurement["x"]) - float(reference[0])
        dy = float(measurement["y"]) - float(reference[1])
        return dx * dx + dy * dy > self._jump_limit(measurement) ** 2

    def _expire(self, reason, timestamp_ms):
        result = self._invalid("PREDICTION_EXPIRED_%s" % reason, timestamp_ms)
        self.confirmed = False
        self.valid_streak = 0
        self.predict_frames = 0
        self.last_real = None
        self.filtered_point = None
        self.last_raw_point = None
        self.kalman.reset()
        return result

    def _predict(self, timestamp_ms, failure_reason):
        if self.last_real is None:
            return self._invalid(failure_reason, timestamp_ms)
        age_ms = max(0, int(timestamp_ms) - int(self.last_real["timestamp_ms"]))
        self.predict_frames += 1
        if age_ms > self.predict_hold_ms or self.predict_frames > self.predict_max_frames:
            return self._expire(failure_reason, timestamp_ms)
        point = self.kalman.predict(timestamp_ms) if self.use_kalman else self.filtered_point
        if point is None:
            point = self.filtered_point
        if point is None:
            return self._expire("INTERNAL_STATE_%s" % failure_reason, timestamp_ms)
        result = dict(self.last_real)
        result.update({
            "valid": True, "updated": False, "predicted": True, "lost_hold": False,
            "x": point[0], "y": point[1], "timestamp_ms": int(timestamp_ms),
            "age_ms": age_ms, "confidence": max(1, int(self.last_real["confidence"]) - 10 * self.predict_frames),
            "reason": "PREDICT_%s" % failure_reason, "mode": "TEMPORAL",
        })
        return result

    def update(self, measurement, timestamp_ms=None):
        timestamp_ms = int(measurement.get("timestamp_ms", 0) if timestamp_ms is None and isinstance(measurement, dict) else timestamp_ms or 0)
        legal, reason = self._is_legal(measurement)
        if not legal:
            return self._predict(timestamp_ms, reason)
        if self._is_outlier(measurement):
            return self._predict(timestamp_ms, "OUTLIER_REJECTED")

        self.predict_frames = 0
        raw_x, raw_y = float(measurement["x"]), float(measurement["y"])
        self.last_raw_point = raw_x, raw_y
        if self.filtered_point is None:
            filtered = raw_x, raw_y
        else:
            filtered = (
                blend(self.filtered_point[0], raw_x, self.lowpass_alpha),
                blend(self.filtered_point[1], raw_y, self.lowpass_alpha),
            )
        self.filtered_point = filtered
        if self.use_kalman:
            filtered = self.kalman.update(filtered[0], filtered[1], timestamp_ms)
            self.filtered_point = filtered
        self.valid_streak += 1
        needed = self.retrack_frames if self.ever_confirmed else self.acquire_frames
        if self.valid_streak < needed:
            return self._invalid("CONFIRM_%d_OF_%d" % (self.valid_streak, needed), timestamp_ms)

        self.confirmed = True
        self.ever_confirmed = True
        result = dict(measurement)
        result.update({
            "valid": True, "updated": True, "predicted": False, "lost_hold": False,
            "x": filtered[0], "y": filtered[1], "timestamp_ms": timestamp_ms,
            "age_ms": 0, "reason": "CONFIRMED", "mode": "TEMPORAL",
        })
        self.last_real = dict(result)
        return result
