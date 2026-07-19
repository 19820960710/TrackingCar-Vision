"""Blue-violet laser detection inside a stable target ROI only."""

import math

try:
    import cv2
    import numpy as np
except ImportError:
    cv2 = None
    np = None

from app.models import new_measurement
from vision.geometry.roi import clip_roi
from vision.laser.background import StaticLightBackground
from vision.laser.gate import accepts
from vision.laser.scorer import candidate_score


class LaserDetector:
    def __init__(self, lab_lower=None, lab_upper=None, min_area=None, max_area=None,
                 min_circularity=None, min_density=None, confirm_frames=None,
                 background_radius=None, background_required_frames=None, relock_radius=None):
        from settings import (
            LASER_BACKGROUND_MATCH_RADIUS, LASER_BACKGROUND_REQUIRED_FRAMES,
            LASER_CONFIRM_FRAMES, LASER_LAB_LOWER, LASER_LAB_UPPER,
            LASER_MAX_AREA, LASER_MIN_AREA, LASER_MIN_CIRCULARITY,
            LASER_MIN_DENSITY, LASER_RELOCK_RADIUS,
        )
        self.lab_lower = lab_lower or LASER_LAB_LOWER
        self.lab_upper = lab_upper or LASER_LAB_UPPER
        self._lab_lower_array = np.array(self.lab_lower, dtype=np.uint8) if np is not None else None
        self._lab_upper_array = np.array(self.lab_upper, dtype=np.uint8) if np is not None else None
        self.min_area = LASER_MIN_AREA if min_area is None else min_area
        self.max_area = LASER_MAX_AREA if max_area is None else max_area
        self.min_circularity = LASER_MIN_CIRCULARITY if min_circularity is None else min_circularity
        self.min_density = LASER_MIN_DENSITY if min_density is None else min_density
        self.confirm_frames = LASER_CONFIRM_FRAMES if confirm_frames is None else confirm_frames
        background_radius = LASER_BACKGROUND_MATCH_RADIUS if background_radius is None else background_radius
        background_required_frames = LASER_BACKGROUND_REQUIRED_FRAMES if background_required_frames is None else background_required_frames
        self.relock_radius = LASER_RELOCK_RADIUS if relock_radius is None else relock_radius
        self.background = StaticLightBackground(background_radius, background_required_frames)
        self.locked = None
        self.pending = None
        self.pending_count = 0

    def _invalid(self, reason, timestamp_ms):
        result = new_measurement("laser")
        result.update({"timestamp_ms": int(timestamp_ms), "reason": reason, "mode": "LASER_LAB", "color": "blue_violet"})
        return result

    def _reset_tracking(self):
        self.locked = None
        self.pending = None
        self.pending_count = 0

    def _stable_target(self, target):
        return bool(target and target.get("valid") and target.get("updated") and not target.get("predicted") and target.get("rect"))

    def _to_bgr_roi(self, frame, roi):
        if hasattr(frame, "shape"):
            x, y, width, height = roi
            return frame[y:y + height, x:x + width]
        from maix import image
        return image.image2cv(frame.crop(*roi), ensure_bgr=True, copy=False)

    def _candidates(self, frame, roi):
        if cv2 is None or self._lab_lower_array is None:
            return []
        try:
            bgr = self._to_bgr_roi(frame, roi)
            if bgr is None or bgr.size == 0:
                return []
            lab = cv2.cvtColor(bgr, cv2.COLOR_BGR2LAB)
            mask = cv2.inRange(lab, self._lab_lower_array, self._lab_upper_array)
            contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
        except Exception:
            return []
        candidates = []
        offset_x, offset_y = roi[0], roi[1]
        for contour in contours:
            area = cv2.contourArea(contour)
            if area < self.min_area or area > self.max_area:
                continue
            perimeter = cv2.arcLength(contour, True)
            if perimeter <= 0:
                continue
            circularity = 4.0 * math.pi * area / (perimeter * perimeter)
            x, y, width, height = cv2.boundingRect(contour)
            density = area / max(1.0, width * height)
            if circularity < self.min_circularity or density < self.min_density:
                continue
            moment = cv2.moments(contour)
            if moment["m00"] == 0:
                continue
            center_x = moment["m10"] / moment["m00"] + offset_x
            center_y = moment["m01"] / moment["m00"] + offset_y
            candidates.append({"x": center_x, "y": center_y, "rect": (x + offset_x, y + offset_y, width, height), "score": candidate_score(area, circularity, density, self.min_area, self.max_area)})
        return candidates

    def _confirm(self, candidate):
        if self.pending is not None:
            dx = candidate["x"] - self.pending["x"]
            dy = candidate["y"] - self.pending["y"]
            if dx * dx + dy * dy <= self.relock_radius * self.relock_radius:
                self.pending = candidate
                self.pending_count += 1
            else:
                self.pending = candidate
                self.pending_count = 1
        else:
            self.pending = candidate
            self.pending_count = 1
        return self.pending_count >= self.confirm_frames

    def _measurement(self, candidate, timestamp_ms, reason):
        return {"kind": "laser", "valid": True, "updated": True, "predicted": False, "lost_hold": False, "x": candidate["x"], "y": candidate["y"], "rect": candidate["rect"], "corners": None, "confidence": max(1, min(100, int(round(candidate["score"] * 100)))), "timestamp_ms": int(timestamp_ms), "age_ms": 0, "reason": reason, "mode": "LASER_LAB", "color": "blue_violet"}

    def detect(self, frame, context, target):
        context = context or {}
        timestamp_ms = int(context.get("timestamp_ms", 0))
        if not self._stable_target(target):
            self._reset_tracking()
            result = self._invalid("TARGET_NOT_STABLE", timestamp_ms)
            return result
        try:
            frame_height, frame_width = frame.shape[:2] if hasattr(frame, "shape") else (frame.height(), frame.width())
            roi = clip_roi(*target["rect"], frame_width, frame_height)
        except (AttributeError, TypeError, ValueError):
            result = self._invalid("INVALID_TARGET_ROI", timestamp_ms)
            return result
        if roi is None:
            result = self._invalid("INVALID_TARGET_ROI", timestamp_ms)
            return result
        candidates = self._candidates(frame, roi)
        if context.get("laser_calibration", False):
            self.background.observe(candidates)
            self._reset_tracking()
            result = self._invalid("BACKGROUND_CALIBRATING" if not self.background.ready() else "BACKGROUND_READY", timestamp_ms)
            return result
        candidates = [candidate for candidate in candidates if not self.background.is_static((candidate["x"], candidate["y"]))]
        if not candidates:
            self.pending = None
            self.pending_count = 0
            result = self._invalid("NO_LASER_CANDIDATE", timestamp_ms)
            return result
        candidate = max(candidates, key=lambda item: item["score"])
        if self.locked is None:
            if not self._confirm(candidate):
                result = self._invalid("LASER_CONFIRM_%d_OF_%d" % (self.pending_count, self.confirm_frames), timestamp_ms)
                return result
            self.locked = self.pending
            self.pending = None
            self.pending_count = 0
            result = self._measurement(self.locked, timestamp_ms, "LASER_LOCKED")
            return result
        if accepts((self.locked["x"], self.locked["y"]), (candidate["x"], candidate["y"]), target["rect"]):
            self.locked = candidate
            self.pending = None
            self.pending_count = 0
            result = self._measurement(candidate, timestamp_ms, "LASER_TRACKED")
            return result
        if not self._confirm(candidate):
            result = self._invalid("LASER_JUMP_CONFIRM_%d_OF_%d" % (self.pending_count, self.confirm_frames), timestamp_ms)
            return result
        self.locked = self.pending
        self.pending = None
        self.pending_count = 0
        result = self._measurement(self.locked, timestamp_ms, "LASER_RELOCKED")
        return result
