"""Measured standard-plane mapping for the white inner target frame."""

from math import sqrt

from vision.geometry.center import quadrilateral_center
from vision.geometry.circle_path import standard_circle
from vision.geometry.homography import compute, invert, project
from vision.geometry.quadrilateral import distance, order_corners


class CalibrationRequired(ValueError):
    pass


class MeasuredTargetDimensions:
    """Physical dimensions in millimetres; never infer white size from A4 size."""

    def __init__(self, white_width_mm, white_height_mm, outer_width_mm=None, outer_height_mm=None):
        if white_width_mm is None or white_height_mm is None:
            raise CalibrationRequired("measure and fill white inner-frame width/height before millimetre mapping")
        self.white_width_mm = float(white_width_mm)
        self.white_height_mm = float(white_height_mm)
        self.outer_width_mm = None if outer_width_mm is None else float(outer_width_mm)
        self.outer_height_mm = None if outer_height_mm is None else float(outer_height_mm)
        if self.white_width_mm <= 0 or self.white_height_mm <= 0:
            raise CalibrationRequired("white inner-frame dimensions must be positive")
        if self.outer_width_mm is not None and self.outer_width_mm < self.white_width_mm:
            raise CalibrationRequired("outer width cannot be smaller than white inner width")
        if self.outer_height_mm is not None and self.outer_height_mm < self.white_height_mm:
            raise CalibrationRequired("outer height cannot be smaller than white inner height")

    @classmethod
    def from_settings(cls):
        from settings import PLANE_OUTER_HEIGHT_MM, PLANE_OUTER_WIDTH_MM, PLANE_WHITE_HEIGHT_MM, PLANE_WHITE_WIDTH_MM
        return cls(PLANE_WHITE_WIDTH_MM, PLANE_WHITE_HEIGHT_MM, PLANE_OUTER_WIDTH_MM, PLANE_OUTER_HEIGHT_MM)

    def white_corners_mm(self):
        return ((0.0, 0.0), (self.white_width_mm, 0.0), (self.white_width_mm, self.white_height_mm), (0.0, self.white_height_mm))

    def center_mm(self):
        return self.white_width_mm * 0.5, self.white_height_mm * 0.5


class StandardPlaneMapper:
    """Map image pixels to measured white-frame millimetres and back.

    Cached circle pixels are reused when each tracked image corner moves no more
    than `cache_corner_epsilon_px`; this prevents needless inverse projection on
    stable target frames.
    """

    def __init__(self, dimensions, circle_radius_mm=60.0, circle_count=50, cache_corner_epsilon_px=0.75):
        self.dimensions = dimensions
        self.circle_radius_mm = float(circle_radius_mm)
        self.circle_count = int(circle_count)
        self.cache_corner_epsilon_px = float(cache_corner_epsilon_px)
        self.image_corners = None
        self.image_to_plane = None
        self.plane_to_image = None
        self._circle_pixels = None

    def _corners_stable(self, corners):
        return self.image_corners is not None and all(
            distance(old, new) <= self.cache_corner_epsilon_px
            for old, new in zip(self.image_corners, corners)
        )

    def update_corners(self, image_corners):
        """Update matrices only if ordered image corners changed materially.

        Returns True when matrices/circle cache were recomputed, False when the
        previous projection remains valid.
        """
        ordered = order_corners(image_corners)
        if self._corners_stable(ordered):
            return False
        self.image_corners = ordered
        self.image_to_plane = compute(ordered, self.dimensions.white_corners_mm())
        self.plane_to_image = invert(self.image_to_plane)
        self._circle_pixels = None
        return True

    def ready(self):
        return self.image_to_plane is not None and self.plane_to_image is not None

    def pixel_to_mm(self, point):
        if not self.ready():
            raise CalibrationRequired("update valid target corners before pixel-to-mm mapping")
        return project(self.image_to_plane, point)

    def mm_to_pixel(self, point_mm):
        if not self.ready():
            raise CalibrationRequired("update valid target corners before mm-to-pixel mapping")
        return project(self.plane_to_image, point_mm)

    def target_center_mm(self):
        return self.dimensions.center_mm()

    def target_center_pixel(self):
        if self.image_corners is None:
            raise CalibrationRequired("update valid target corners before target center mapping")
        return quadrilateral_center(self.image_corners)

    def measurement_to_mm(self, measurement):
        if not measurement or not measurement.get("valid"):
            return None
        return self.pixel_to_mm((measurement["x"], measurement["y"]))

    def circle_points_mm(self):
        center_x, center_y = self.dimensions.center_mm()
        return standard_circle(center_x, center_y, self.circle_radius_mm, self.circle_count)

    def circle_points_pixel(self):
        if not self.ready():
            raise CalibrationRequired("update valid target corners before circle projection")
        if self._circle_pixels is None:
            self._circle_pixels = tuple(self.mm_to_pixel(point) for point in self.circle_points_mm())
        return self._circle_pixels

    def circle_radius_error_mm(self, image_points):
        """Return maximum radial error after mapping image circle points to mm."""
        center = self.dimensions.center_mm()
        errors = []
        for point in image_points:
            x, y = self.pixel_to_mm(point)
            errors.append(abs(sqrt((x - center[0]) ** 2 + (y - center[1]) ** 2) - self.circle_radius_mm))
        return max(errors) if errors else 0.0
