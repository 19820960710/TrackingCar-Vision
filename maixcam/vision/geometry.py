from settings import *
def frame_center():
    return CAMERA_WIDTH // 2, CAMERA_HEIGHT // 2


def get_roi():
    roi_w = CAMERA_WIDTH * ROI_SCALE_NUM // ROI_SCALE_DEN
    roi_h = CAMERA_HEIGHT * ROI_SCALE_NUM // ROI_SCALE_DEN
    roi_x = (CAMERA_WIDTH - roi_w) // 2
    roi_y = (CAMERA_HEIGHT - roi_h) // 2
    return roi_x, roi_y, roi_w, roi_h


def point_in_roi(x, y):
    roi_x, roi_y, roi_w, roi_h = get_roi()
    return roi_x <= x <= roi_x + roi_w and roi_y <= y <= roi_y + roi_h


def clamp_rect(x, y, w, h):
    left = max(0, min(CAMERA_WIDTH - 1, int(x)))
    top = max(0, min(CAMERA_HEIGHT - 1, int(y)))
    right = max(left + 1, min(CAMERA_WIDTH, int(x + w)))
    bottom = max(top + 1, min(CAMERA_HEIGHT, int(y + h)))
    return left, top, right - left, bottom - top


def point_in_rect(x, y, rect):
    return rect[0] <= x <= rect[0] + rect[2] and rect[1] <= y <= rect[1] + rect[3]



def safe_magnitude(obj):
    try:
        return obj.magnitude()
    except Exception:
        return 0


def blob_rect(blob):
    try:
        return blob.rect()
    except Exception:
        pass

    try:
        return [blob[0], blob[1], blob[2], blob[3]]
    except Exception:
        pass

    return [blob.x(), blob.y(), blob.w(), blob.h()]


def rect_center(rect):
    return rect[0] + rect[2] // 2, rect[1] + rect[3] // 2


def blob_center(blob, rect):
    if TARGET_BLOB_CENTER_METHOD == "rect":
        return rect_center(rect)

    try:
        return blob.cx(), blob.cy()
    except Exception:
        return rect_center(rect)


def laser_blob_center(blob, rect):
    if LASER_CENTER_METHOD == "rect":
        return rect_center(rect)

    try:
        return blob.cx(), blob.cy()
    except Exception:
        return rect_center(rect)


def rect_area(rect):
    return rect[2] * rect[3]


def blob_pixels(blob, fallback):
    try:
        return int(blob.pixels())
    except Exception:
        return fallback


def blob_ratio_x100(blob, method_name, fallback_x100):
    try:
        method = getattr(blob, method_name)
        return int(method() * 100)
    except Exception:
        return fallback_x100


def distance_xy(x1, y1, x2, y2):
    return abs(x1 - x2) + abs(y1 - y2)


def point_xy(point):
    try:
        return [int(point[0]), int(point[1])]
    except Exception:
        return [int(point.x()), int(point.y())]


def order_corners(corners):
    points = [point_xy(point) for point in corners]
    if len(points) != 4:
        return None

    sums = [point[0] + point[1] for point in points]
    diffs = [point[1] - point[0] for point in points]
    top_left = points[sums.index(min(sums))]
    bottom_right = points[sums.index(max(sums))]
    top_right = points[diffs.index(min(diffs))]
    bottom_left = points[diffs.index(max(diffs))]
    ordered = [top_left, top_right, bottom_right, bottom_left]

    seen = []
    for point in ordered:
        key = "%d,%d" % (point[0], point[1])
        if key in seen:
            return None
        seen.append(key)
    return ordered


def quad_bounds(corners):
    xs = [point[0] for point in corners]
    ys = [point[1] for point in corners]
    left = min(xs)
    top = min(ys)
    right = max(xs)
    bottom = max(ys)
    return [left, top, right - left, bottom - top]


def quad_area(corners):
    area2 = 0
    for i in range(4):
        x1, y1 = corners[i]
        x2, y2 = corners[(i + 1) % 4]
        area2 += x1 * y2 - x2 * y1
    return abs(area2) // 2


def diagonal_center(corners):
    x1, y1 = corners[0]
    x2, y2 = corners[2]
    x3, y3 = corners[1]
    x4, y4 = corners[3]

    den = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4)
    if den == 0:
        x = (corners[0][0] + corners[1][0] + corners[2][0] + corners[3][0]) // 4
        y = (corners[0][1] + corners[1][1] + corners[2][1] + corners[3][1]) // 4
        return x, y

    pre = x1 * y2 - y1 * x2
    post = x3 * y4 - y3 * x4
    x = (pre * (x3 - x4) - (x1 - x2) * post) // den
    y = (pre * (y3 - y4) - (y1 - y2) * post) // den
    return int(x), int(y)


def safe_rect_corners(rect_obj):
    try:
        return order_corners(rect_obj.corners())
    except Exception:
        return None

def weighted_value(old_value, new_value, alpha):
    return (old_value * (100 - alpha) + new_value * alpha) // 100


def smooth_value(old_value, new_value):
    return weighted_value(old_value, new_value, TARGET_SMOOTHING_ALPHA_X100)
