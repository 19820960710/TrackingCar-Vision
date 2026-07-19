import unittest

from app.pipeline import VisionPipeline


class BrokenTargetDetector:
    def detect(self, _frame, _context):
        raise RuntimeError("camera processing failure")


class CollectingUart:
    def __init__(self):
        self.frames = []

    def write_str(self, frame):
        self.frames.append(frame)


class PipelineSafetyTest(unittest.TestCase):
    def test_target_exception_becomes_invalid_and_uart_gets_lost(self):
        uart = CollectingUart()
        pipeline = VisionPipeline(BrokenTargetDetector(), protocol=lambda target, laser: "LOST\n" if not target["valid"] else "BAD\n", uart_device=uart)
        observation = pipeline.process(None, {"timestamp_ms": 10})
        self.assertFalse(observation["target"]["valid"])
        self.assertIn("PIPELINE_TARGET_RuntimeError", observation["target"]["reason"])
        self.assertTrue(observation["uart"]["ok"])
        self.assertEqual(uart.frames, ["LOST\n"])


if __name__ == "__main__":
    unittest.main()
