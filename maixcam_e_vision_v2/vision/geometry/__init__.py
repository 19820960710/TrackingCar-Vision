"""Pure geometry helpers for target validation and measured coordinate mapping."""

from vision.geometry.center import diagonal_intersection, quadrilateral_center
from vision.geometry.homography import compute, invert, project
from vision.geometry.quadrilateral import distance, is_convex_quad, order_corners, quadrilateral_metrics
from vision.geometry.roi import clip_roi
from vision.geometry.standard_plane import CalibrationRequired, MeasuredTargetDimensions, StandardPlaneMapper

__all__ = (
    "CalibrationRequired", "MeasuredTargetDimensions", "StandardPlaneMapper",
    "clip_roi", "compute", "diagonal_intersection", "distance", "invert",
    "is_convex_quad", "order_corners", "project", "quadrilateral_center",
    "quadrilateral_metrics",
)
