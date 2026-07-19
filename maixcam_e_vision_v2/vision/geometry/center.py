"""Diagonal-intersection target center."""

from vision.geometry.quadrilateral import order_corners


def diagonal_intersection(tl, tr, br, bl):
    x1, y1 = tl
    x2, y2 = br
    x3, y3 = tr
    x4, y4 = bl
    denominator = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4)
    if denominator == 0:
        return None
    a = x1 * y2 - y1 * x2
    b = x3 * y4 - y3 * x4
    return ((a * (x3 - x4) - (x1 - x2) * b) / denominator, (a * (y3 - y4) - (y1 - y2) * b) / denominator)


def quadrilateral_center(points):
    """Order arbitrary quad corners and return their diagonal intersection."""
    tl, tr, br, bl = order_corners(points)
    return diagonal_intersection(tl, tr, br, bl)
