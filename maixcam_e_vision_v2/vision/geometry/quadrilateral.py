"""Pure quadrilateral geometry helpers, independent of camera and OpenCV."""

import math


EPSILON = 1e-9


def distance(a, b):
    """Return Euclidean distance between two 2D points."""
    dx = a[0] - b[0]
    dy = a[1] - b[1]
    return math.sqrt(dx * dx + dy * dy)


def _valid_points(points):
    if not points or len(points) != 4:
        return False
    try:
        normalized = [(float(point[0]), float(point[1])) for point in points]
    except (IndexError, TypeError, ValueError):
        return False
    return all(math.isfinite(x) and math.isfinite(y) for x, y in normalized) and len(set(normalized)) == 4


def _cross(a, b, c):
    return (b[0] - a[0]) * (c[1] - b[1]) - (b[1] - a[1]) * (c[0] - b[0])


def order_corners(points):
    """Return corners in image order: top-left, top-right, bottom-right, bottom-left.

    Input order is arbitrary.  In image coordinates the returned loop has a
    positive cross product (x right, y down).  Degenerate point sets raise
    ValueError instead of silently producing an unsafe target center.
    """
    if not _valid_points(points):
        raise ValueError("exactly four distinct finite 2D points are required")
    normalized = [(float(point[0]), float(point[1])) for point in points]
    center_x = sum(point[0] for point in normalized) / 4.0
    center_y = sum(point[1] for point in normalized) / 4.0
    loop = sorted(normalized, key=lambda point: math.atan2(point[1] - center_y, point[0] - center_x))
    start = min(range(4), key=lambda index: (loop[index][0] + loop[index][1], loop[index][1], loop[index][0]))
    loop = loop[start:] + loop[:start]
    if _cross(loop[0], loop[1], loop[2]) < 0:
        loop = [loop[0], loop[3], loop[2], loop[1]]
    if not _is_convex_ordered(loop):
        raise ValueError("points do not form a non-degenerate convex quadrilateral")
    return tuple(loop)


def _is_convex_ordered(points):
    signs = []
    for index in range(4):
        cross = _cross(points[index], points[(index + 1) % 4], points[(index + 2) % 4])
        if abs(cross) <= EPSILON:
            return False
        signs.append(cross > 0)
    return all(signs) or not any(signs)


def is_convex_quad(points):
    """Return whether four arbitrary points form a non-degenerate convex quad."""
    try:
        order_corners(points)
    except ValueError:
        return False
    return True


def _angle_degrees(previous, vertex, following):
    ax = previous[0] - vertex[0]
    ay = previous[1] - vertex[1]
    bx = following[0] - vertex[0]
    by = following[1] - vertex[1]
    denominator = math.sqrt(ax * ax + ay * ay) * math.sqrt(bx * bx + by * by)
    if denominator <= EPSILON:
        raise ValueError("zero-length side")
    cosine = max(-1.0, min(1.0, (ax * bx + ay * by) / denominator))
    return math.degrees(math.acos(cosine))


def quadrilateral_metrics(points):
    """Return ordered corners, side lengths, inner angles and opposite-side ratios."""
    tl, tr, br, bl = order_corners(points)
    lengths = {
        "top": distance(tl, tr),
        "right": distance(tr, br),
        "bottom": distance(br, bl),
        "left": distance(bl, tl),
    }
    return {
        "corners": (tl, tr, br, bl),
        "side_lengths": lengths,
        "angles_deg": {
            "tl": _angle_degrees(bl, tl, tr),
            "tr": _angle_degrees(tl, tr, br),
            "br": _angle_degrees(tr, br, bl),
            "bl": _angle_degrees(br, bl, tl),
        },
        "opposite_side_ratios": {
            "top_to_bottom": lengths["top"] / lengths["bottom"],
            "left_to_right": lengths["left"] / lengths["right"],
        },
    }
