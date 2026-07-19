import unittest

from vision.geometry.center import diagonal_intersection, quadrilateral_center
from vision.geometry.homography import compute, invert, project
from vision.geometry.quadrilateral import is_convex_quad, order_corners, quadrilateral_metrics
from vision.geometry.roi import clip_roi


class GeometryTest(unittest.TestCase):
    def assertPointAlmostEqual(self, actual, expected, places=6):
        self.assertAlmostEqual(actual[0], expected[0], places=places)
        self.assertAlmostEqual(actual[1], expected[1], places=places)

    def test_orders_arbitrary_corners_and_finds_center(self):
        corners = [(100, 40), (0, 0), (0, 40), (100, 0)]
        self.assertEqual(order_corners(corners), ((0.0, 0.0), (100.0, 0.0), (100.0, 40.0), (0.0, 40.0)))
        self.assertEqual(quadrilateral_center(corners), (50.0, 20.0))

    def test_diagonal_intersection(self):
        self.assertEqual(diagonal_intersection((0, 0), (10, 0), (10, 20), (0, 20)), (5.0, 10.0))

    def test_convex_and_illegal_quadrilaterals(self):
        self.assertTrue(is_convex_quad([(0, 0), (10, 0), (10, 10), (0, 10)]))
        self.assertFalse(is_convex_quad([(0, 0), (10, 0), (4, 3), (0, 10)]))
        self.assertFalse(is_convex_quad([(0, 0), (10, 0), (20, 0), (0, 10)]))
        self.assertFalse(is_convex_quad([(0, 0), (10, 0), (10, 0), (0, 10)]))

    def test_side_angles_and_opposite_ratios(self):
        metrics = quadrilateral_metrics([(10, 20), (0, 0), (10, 0), (0, 20)])
        self.assertEqual(metrics["side_lengths"], {"top": 10.0, "right": 20.0, "bottom": 10.0, "left": 20.0})
        for angle in metrics["angles_deg"].values():
            self.assertAlmostEqual(angle, 90.0)
        self.assertEqual(metrics["opposite_side_ratios"], {"top_to_bottom": 1.0, "left_to_right": 1.0})

    def test_roi_clip(self):
        self.assertEqual(clip_roi(-5, 10, 20, 20, 100, 50), (0, 10, 15, 20))
        self.assertEqual(clip_roi(90.2, 40.2, 20, 20, 100, 50), (90, 40, 10, 10))
        self.assertIsNone(clip_roi(101, 0, 10, 10, 100, 50))

    def test_homography_and_inverse_projection(self):
        image = ((0, 0), (100, 0), (100, 50), (0, 50))
        plane = ((0, 0), (200, 0), (200, 100), (0, 100))
        image_to_plane = compute(image, plane)
        self.assertPointAlmostEqual(project(image_to_plane, (50, 25)), (100, 50))
        self.assertPointAlmostEqual(project(invert(image_to_plane), (100, 50)), (50, 25))


if __name__ == "__main__":
    unittest.main()
