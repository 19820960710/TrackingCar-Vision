"""One-frame V2 visual scheduling: target, laser, geometry, UART and timing."""

from app.models import new_measurement, new_observation, normalize_measurement
from app.timing import PerformanceStats, elapsed_us, now_us
from settings import PERFORMANCE_SNAPSHOT_INTERVAL


class VisionPipeline:
    def __init__(self, target_detector=None, laser_detector=None, protocol=None, uart_device=None,
                 plane_mapper=None, performance=None, performance_snapshot_interval=None):
        self.target_detector = target_detector
        self.laser_detector = laser_detector
        self.protocol = protocol
        self.uart_device = uart_device
        self.plane_mapper = plane_mapper
        self.performance = performance or PerformanceStats()
        interval = PERFORMANCE_SNAPSHOT_INTERVAL if performance_snapshot_interval is None else performance_snapshot_interval
        self.performance_snapshot_interval = max(1, int(interval))
        self.processed_frames = 0
        self.last_performance_snapshot = None

    def _timed(self, name, function):
        started = now_us()
        value = function()
        self.performance.add(name, elapsed_us(now_us(), started))
        return value

    def _safe_timed_measurement(self, name, kind, function, timestamp_ms):
        started = now_us()
        try:
            return function()
        except Exception as error:
            result = new_measurement(kind)
            result.update({"timestamp_ms": int(timestamp_ms), "reason": "PIPELINE_%s_%s" % (name.upper(), type(error).__name__)})
            return result
        finally:
            self.performance.add(name, elapsed_us(now_us(), started))

    def _encode(self, target, laser):
        if self.protocol is None:
            return None
        encoder = self.protocol if callable(self.protocol) else self.protocol.encode
        return encoder(target, laser)

    def _geometry(self, target, laser, context):
        if self.plane_mapper is None or not context.get("geometry_enabled", False):
            return {"valid": False, "reason": "GEOMETRY_DISABLED"}
        if not target.get("valid") or not target.get("updated") or target.get("predicted") or not target.get("corners"):
            return {"valid": False, "reason": "TARGET_NOT_READY"}
        try:
            recomputed = self.plane_mapper.update_corners(target["corners"])
            result = {"valid": True, "recomputed": recomputed, "target_mm": self.plane_mapper.measurement_to_mm(target), "laser_mm": self.plane_mapper.measurement_to_mm(laser)}
            if context.get("circle_requested", False):
                result["circle_points_px"] = self.plane_mapper.circle_points_pixel()
            return result
        except Exception as error:
            return {"valid": False, "reason": "GEOMETRY_%s" % type(error).__name__}

    def _write_uart(self, frame):
        if self.uart_device is None or frame is None:
            return {"attempted": False, "ok": False, "reason": "UART_DISABLED"}
        try:
            if isinstance(frame, bytes):
                self.uart_device.write(frame)
            else:
                self.uart_device.write_str(frame)
            return {"attempted": True, "ok": True, "reason": "OK"}
        except Exception as error:
            return {"attempted": True, "ok": False, "reason": "UART_%s" % type(error).__name__}

    def process(self, frame, context):
        context = context or {}
        timestamp_ms = context.get("timestamp_ms", 0)
        frame_started = now_us()
        target_raw = self._safe_timed_measurement("target", "target", lambda: self.target_detector.detect(frame, context), timestamp_ms) if self.target_detector else None
        target = normalize_measurement("target", target_raw)
        ai_us = getattr(self.target_detector, "last_ai_recapture_us", None) if self.target_detector else None
        if ai_us is not None:
            self.performance.add("ai_recapture", ai_us)
        laser_raw = self._safe_timed_measurement("laser", "laser", lambda: self.laser_detector.detect(frame, context, target), timestamp_ms) if self.laser_detector else None
        laser = normalize_measurement("laser", laser_raw)
        observation = new_observation(target, laser, context.get("frame_index", 0), timestamp_ms)
        observation["target_state"] = getattr(self.target_detector, "last_state", None) if self.target_detector else None
        observation["geometry"] = self._timed("geometry", lambda: self._geometry(target, laser, context))
        try:
            uart_frame = self._timed("uart_encode", lambda: self._encode(target, laser))
        except Exception as error:
            uart_frame = None
            observation["uart_encode_error"] = "UART_ENCODE_%s" % type(error).__name__
        observation["uart"] = self._timed("uart_write", lambda: self._write_uart(uart_frame))
        self.performance.add("frame_cpu", elapsed_us(now_us(), frame_started))
        self.processed_frames += 1
        if self.last_performance_snapshot is None or self.processed_frames % self.performance_snapshot_interval == 0:
            self.last_performance_snapshot = self.performance.snapshot()
        observation["performance"] = self.last_performance_snapshot
        return observation
