import unittest

from app.pipeline import VisionPipeline
from app.runtime import VisionRuntime
from drivers.display import DisplayDevice, draw_debug


class FakeColors:
    COLOR_RED = 1
    COLOR_GREEN = 2
    COLOR_BLUE = 3
    COLOR_YELLOW = 4


class FakeFrame:
    def __init__(self):
        self.calls = []

    def draw_line(self, *args):
        self.calls.append(("line", args))

    def draw_rect(self, *args):
        self.calls.append(("rect", args))

    def draw_circle(self, *args):
        self.calls.append(("circle", args))

    def draw_string(self, *args):
        self.calls.append(("string", args))


class FakeScreen:
    def __init__(self):
        self.frames = []

    def show(self, frame):
        self.frames.append(frame)


class FakeCamera:
    def __init__(self, frame):
        self.frame = frame

    def read(self):
        return self.frame


class CollectingDisplay:
    def __init__(self):
        self.calls = []

    def show(self, frame, observation):
        self.calls.append((frame, observation))


class DisplayTest(unittest.TestCase):
    def _observation(self):
        return {
            "target": {
                "valid": True,
                "predicted": False,
                "x": 30.4,
                "y": 40.6,
                "confidence": 87,
                "rect": (10, 20, 40, 50),
                "corners": ((10, 20), (50, 20), (50, 70), (10, 70)),
            },
            "laser": {"valid": True, "x": 32, "y": 39, "reason": "LASER_LOCKED"},
            "performance": {
                "target": {"p95_us": 3200},
                "frame_cpu": {"p95_us": 5100},
                "uart_write": {"p95_us": 100},
            },
        }

    def test_debug_overlay_draws_target_laser_fps_and_timing(self):
        frame = FakeFrame()

        rendered = draw_debug(frame, self._observation(), True, 42.5, FakeColors)

        self.assertIs(rendered, frame)
        red_lines = [call for call in frame.calls if call[0] == "line" and call[1][-2] == FakeColors.COLOR_RED]
        self.assertGreaterEqual(len(red_lines), 6)
        texts = [call[1][2] for call in frame.calls if call[0] == "string"]
        self.assertIn("FPS: 42.5", texts)
        self.assertTrue(any(text.startswith("TARGET: LOCK 87%") for text in texts))
        self.assertTrue(any(text.startswith("LASER: LOCK") for text in texts))
        self.assertTrue(any(text.startswith("P95 ms") for text in texts))
        red_cross = ("line", (22, 45, 38, 45, FakeColors.COLOR_RED, 2))
        self.assertIn(red_cross, frame.calls)
        self.assertIn(("line", (30, 41, 32, 39, FakeColors.COLOR_GREEN, 1)), frame.calls)

    def test_compact_locked_overlay_draws_only_fps_text(self):
        frame = FakeFrame()

        draw_debug(frame, self._observation(), True, 30.0, FakeColors, False)

        texts = [call[1][2] for call in frame.calls if call[0] == "string"]
        self.assertEqual(texts, ["FPS: 30.0"])

    def test_debug_overlay_shows_recovery_reason_when_target_is_lost(self):
        frame = FakeFrame()
        observation = {
            "target": {"valid": False, "reason": "RAW_INVALID"},
            "target_state": {"reason": "NO_BLACK_FRAME"},
            "laser": {"valid": False, "reason": "TARGET_NOT_STABLE"},
        }

        draw_debug(frame, observation, True, 10.0, FakeColors)

        texts = [call[1][2] for call in frame.calls if call[0] == "string"]
        self.assertIn("TARGET: LOST NO_BLACK_FRAME", texts)
        self.assertIn("LASER: LOST TARGET_NOT_STABLE", texts)

    def test_display_device_presents_frame_and_calculates_fps(self):
        frame = FakeFrame()
        screen = FakeScreen()
        ticks = iter((1000000, 1020000))
        display_device = DisplayDevice(
            screen=screen,
            debug_enabled=True,
            clock_us=lambda: next(ticks),
            colors=FakeColors,
        )

        display_device.show(frame, self._observation())
        display_device.show(frame, self._observation())

        self.assertEqual(screen.frames, [frame, frame])
        self.assertAlmostEqual(display_device.fps, 50.0)
        texts = [call[1][2] for call in frame.calls if call[0] == "string"]
        self.assertIn("FPS: 50.0", texts)

    def test_runtime_presents_each_captured_frame(self):
        frame = object()
        display_device = CollectingDisplay()
        runtime = VisionRuntime(FakeCamera(frame), VisionPipeline(), display_device)

        observation = runtime.run_once({"timestamp_ms": 2000})

        self.assertEqual(len(display_device.calls), 1)
        shown_frame, shown_observation = display_device.calls[0]
        self.assertIs(shown_frame, frame)
        self.assertIs(shown_observation, observation)


if __name__ == "__main__":
    unittest.main()
