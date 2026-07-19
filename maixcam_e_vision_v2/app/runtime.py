"""Camera-facing runtime that keeps acquisition timing outside visual algorithms."""

from app.timing import elapsed_us, now_ms, now_us


class VisionRuntime:
    def __init__(self, camera_device, pipeline, display_device=None):
        self.camera_device = camera_device
        self.pipeline = pipeline
        self.display_device = display_device
        self.frame_index = 0

    def run_once(self, context=None):
        context = dict(context or {})
        frame_started = now_us()
        capture_started = frame_started
        frame = self.camera_device.read()
        self.pipeline.performance.add("capture", elapsed_us(now_us(), capture_started))
        self.frame_index += 1
        context.setdefault("frame_index", self.frame_index)
        context.setdefault("timestamp_ms", now_ms())
        observation = self.pipeline.process(frame, context)
        if "performance" not in observation:
            observation["performance"] = self.pipeline.performance.snapshot()
        if self.display_device is not None:
            display_started = now_us()
            self.display_device.show(frame, observation)
            self.pipeline.performance.add("display", elapsed_us(now_us(), display_started))
        self.pipeline.performance.add("frame_total", elapsed_us(now_us(), frame_started))
        return observation
