import unittest

from vision.geometry.standard_plane import CalibrationRequired, MeasuredTargetDimensions, StandardPlaneMapper


class StandardPlaneTest(unittest.TestCase):
    def assertPointAlmostEqual(self, actual, expected, places=5):
        self.assertAlmostEqual(actual[0], expected[0], places=places)
        self.assertAlmostEqual(actual[1], expected[1], places=places)

    def setUp(self):
        self.dimensions = MeasuredTargetDimensions(174.0, 261.0, 210.0, 297.0)
        self.corners = ((100, 50), (500, 70), (480, 370), (120, 350))
        self.mapper = StandardPlaneMapper(self.dimensions, circle_radius_mm=60.0, circle_count=50)

    def test_requires_measured_white_dimensions(self):
        with self.assertRaises(CalibrationRequired):
            MeasuredTargetDimensions(None, 261.0)

    def test_corners_center_and_pixel_mm_round_trip(self):
        self.assertTrue(self.mapper.update_corners(self.corners))
        self.assertPointAlmostEqual(self.mapper.pixel_to_mm(self.corners[0]), (0, 0))
        self.assertPointAlmostEqual(self.mapper.pixel_to_mm(self.corners[2]), (174, 261))
        center_px = self.mapper.target_center_pixel()
        self.assertPointAlmostEqual(self.mapper.pixel_to_mm(center_px), (87, 130.5))
        self.assertPointAlmostEqual(self.mapper.mm_to_pixel((87, 130.5)), center_px)

    def test_laser_measurement_and_circle_projection(self):
        self.mapper.update_corners(self.corners)
        center_px = self.mapper.target_center_pixel()
        self.assertPointAlmostEqual(self.mapper.measurement_to_mm({"valid": True, "x": center_px[0], "y": center_px[1]}), (87, 130.5))
        pixels = self.mapper.circle_points_pixel()
        self.assertEqual(len(pixels), 50)
        self.assertLess(self.mapper.circle_radius_error_mm(pixels), 1e-5)

    def test_stable_corners_reuse_circle_cache(self):
        self.mapper.update_corners(self.corners)
        cached = self.mapper.circle_points_pixel()
        slightly_moved = tuple((x + 0.2, y - 0.2) for x, y in self.corners)
        self.assertFalse(self.mapper.update_corners(slightly_moved))
        self.assertIs(self.mapper.circle_points_pixel(), cached)
        moved = tuple((x + 3, y) for x, y in self.corners)
        self.assertTrue(self.mapper.update_corners(moved))
        self.assertIsNot(self.mapper.circle_points_pixel(), cached)


if __name__ == "__main__":
    unittest.main()
