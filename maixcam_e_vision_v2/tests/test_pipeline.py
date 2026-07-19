import unittest

from app.models import MEASUREMENT_FIELDS
from app.pipeline import VisionPipeline
from app.runtime import VisionRuntime
from app.timing import PerformanceStats


class FakeTargetDetector:
    last_ai_recapture_us = 321
    last_state = {"mode": "TRACK_ROI_REFINE", "reason": "ROI_REFINE_OK", "timestamp_ms": 1000}

    def detect(self, _frame, _context):
        return {"valid": True, "updated": True, "x": 100, "y": 80, "mode": "TRACK", "rect": (80, 60, 40, 40), "corners": ((80, 60), (120, 60), (120, 100), (80, 100))}


class FakeLaserDetector:
    def detect(self, _frame, _context, target):
        assert set(target) == set(MEASUREMENT_FIELDS)
        return {"valid": True, "updated": True, "x": 105, "y": 78, "color": "blue_violet"}


class FakeUart:
    def __init__(self):
        self.sent = []

    def write_str(self, value):
        self.sent.append(value)


class FakeCamera:
    def read(self):
        return object()


class FakeDisplay:
    def __init__(self):
        self.frames = 0

    def show(self, _frame, _observation):
        self.frames += 1

class CountingPerformance(PerformanceStats):
    def __init__(self):
        super().__init__()
        self.snapshot_calls = 0

    def snapshot(self):
        self.snapshot_calls += 1
        return super().snapshot()


class PipelineTest(unittest.TestCase):
    def test_pipeline_orders_target_laser_geometry_uart_and_timing(self):
        uart = FakeUart()
        pipeline = VisionPipeline(FakeTargetDetector(), FakeLaserDetector(), protocol=lambda target, laser: "AIM,1,0,0,0,0,0,0,TEST,TEST\n", uart_device=uart)
        observation = pipeline.process(None, {"frame_index": 9, "timestamp_ms": 1000})
        self.assertEqual(observation["frame_index"], 9)
        self.assertEqual(observation["target"]["kind"], "target")
        self.assertEqual(observation["laser"]["kind"], "laser")
        self.assertEqual(observation["laser"]["color"], "blue_violet")
        self.assertTrue(observation["uart"]["ok"])
        self.assertEqual(len(uart.sent), 1)
        self.assertEqual(observation["geometry"]["reason"], "GEOMETRY_DISABLED")
        self.assertEqual(observation["performance"]["ai_recapture"]["count"], 1)

    def test_runtime_records_capture_timing(self):
        pipeline = VisionPipeline()
        observation = VisionRuntime(FakeCamera(), pipeline).run_once({"timestamp_ms": 2000})
        self.assertEqual(observation["frame_index"], 1)
        self.assertEqual(observation["performance"]["capture"]["count"], 1)

    def test_runtime_records_display_and_total_frame_timing(self):
        pipeline = VisionPipeline()
        display = FakeDisplay()

        VisionRuntime(FakeCamera(), pipeline, display).run_once({"timestamp_ms": 2000})

        self.assertEqual(display.frames, 1)
        self.assertEqual(pipeline.performance.summary("display")["count"], 1)
        self.assertEqual(pipeline.performance.summary("frame_total")["count"], 1)
    def test_performance_snapshot_is_cached_and_runtime_does_not_duplicate_it(self):
        performance = CountingPerformance()
        pipeline = VisionPipeline(performance=performance, performance_snapshot_interval=10)
        runtime = VisionRuntime(FakeCamera(), pipeline)

        for frame_index in range(11):
            runtime.run_once({"frame_index": frame_index + 1, "timestamp_ms": 2000 + frame_index})

        self.assertEqual(performance.snapshot_calls, 2)


if __name__ == "__main__":
    unittest.main()
