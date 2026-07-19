import unittest

from tools.benchmark import TimingWindow, percentile


class BenchmarkTest(unittest.TestCase):
    def test_percentiles(self):
        values = [10, 20, 30, 40, 50]
        self.assertEqual(percentile(values, 0.50), 30)
        self.assertEqual(percentile(values, 0.95), 40)

    def test_window_capacity_and_summary(self):
        window = TimingWindow(3)
        for value in (10, 20, 30, 40):
            window.add("frame", value)
        self.assertEqual(window.summary("frame"), {"count": 3, "p50_us": 30, "p95_us": 30})


if __name__ == "__main__":
    unittest.main()
