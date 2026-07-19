import unittest

from config import (
    CAMERA_BENCHMARK_CANDIDATES,
    CAMERA_BENCHMARK_INDEX,
    CAMERA_FPS,
    CAMERA_HEIGHT,
    CAMERA_WIDTH,
    UART_TEST_FRAME,
)
from tools.hardware_baseline import selected_candidate


class HardwareBaselineContractTests(unittest.TestCase):
    def test_selected_candidate_comes_from_declared_list(self):
        self.assertEqual(
            selected_candidate(),
            CAMERA_BENCHMARK_CANDIDATES[CAMERA_BENCHMARK_INDEX],
        )

    def test_production_camera_uses_selected_60hz_profile(self):
        self.assertEqual(
            (CAMERA_WIDTH, CAMERA_HEIGHT, CAMERA_FPS),
            CAMERA_BENCHMARK_CANDIDATES[CAMERA_BENCHMARK_INDEX],
        )
    def test_uart_test_frame_is_a_safe_lost_frame(self):
        fields = UART_TEST_FRAME.strip().split(",")
        self.assertEqual(fields[0], "AIM")
        self.assertEqual(fields[1], "0")
        self.assertEqual(fields[8], "LOST")
        self.assertEqual(fields[9], "LOST")


if __name__ == "__main__":
    unittest.main()
