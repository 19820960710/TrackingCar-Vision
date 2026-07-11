from maix import app, camera, display, image, time

try:
    from config import (
        CAMERA_HEIGHT,
        CAMERA_BUFFER_NUM,
        CAMERA_CONTRAST,
        CAMERA_EXPOSURE,
        CAMERA_GAIN,
        CAMERA_SKIP_FRAMES,
        CAMERA_WB_GAIN,
        CAMERA_WIDTH,
        CROSSHAIR_SIZE,
        DETECT_EVERY_N_FRAMES,
        ENABLE_TARGET_DETECT,
        GRID_LINE_WIDTH,
        ENABLE_LASER_DETECT,
        LASER_AREA_MAX,
        LASER_AREA_MIN,
        LASER_COLOR,
        LASER_CONFIRM_DISTANCE,
        LASER_CONFIRM_FRAMES,
        LASER_GREEN_THRESHOLDS,
        LASER_MAX_ASPECT_X100,
        LASER_MAX_H,
        LASER_MAX_W,
        LASER_MIN_H,
        LASER_MIN_W,
        LASER_PIXELS_MIN,
        LASER_RED_THRESHOLDS,
        LASER_USE_ROI,
        PRINT_FPS,
        PRINT_LASER,
        PRINT_TARGET,
        ROI_SCALE_DEN,
        ROI_SCALE_NUM,
        SHOW_CENTER_GUIDE,
        SHOW_FPS,
        SHOW_GRID,
        SHOW_ROI,
        SHOW_STATUS_TEXT,
        SHOW_TARGET_BOX,
        TARGET_BLOB_CENTER_METHOD,
        TARGET_MARKER_BOX_MAX_H,
        TARGET_MARKER_BOX_MAX_W,
        TARGET_MARKER_BOX_MIN_H,
        TARGET_MARKER_BOX_MIN_W,
        TARGET_MARKER_BOX_PADDING,
        TARGET_CIRCLE_THRESHOLD,
        TARGET_BLOB_AREA_MIN,
        TARGET_BLOB_MAX_ASPECT_X100,
        TARGET_BLOB_MIN_H,
        TARGET_BLOB_MIN_W,
        TARGET_BLOB_PIXELS_MIN,
        TARGET_BLOB_THRESHOLDS,
        TARGET_FAST_ROI_ENABLE,
        TARGET_FAST_ROI_PADDING,
        TARGET_FULL_SCAN_INTERVAL,
        TARGET_JUMP_REJECT_ENABLE,
        TARGET_LOST_HOLD_FRAMES,
        TARGET_MAX_CENTER_JUMP,
        TARGET_MAX_RADIUS,
        TARGET_MIN_RADIUS,
        TARGET_MIN_RECT_H,
        TARGET_MIN_RECT_W,
        TARGET_MODE,
        TARGET_PERSPECTIVE_FALLBACK_BLOB,
        TARGET_PERSPECTIVE_DISTANCE_WEIGHT,
        TARGET_PERSPECTIVE_MAX_ASPECT_X100,
        TARGET_PERSPECTIVE_MIN_AREA,
        TARGET_PERSPECTIVE_MIN_H,
        TARGET_PERSPECTIVE_MIN_W,
        TARGET_RECT_THRESHOLD,
        TARGET_ROUGH_FAST_MOVE_DISTANCE,
        TARGET_ROUGH_FIRST_ENABLE,
        TARGET_ROUGH_OUTPUT_ENABLE,
        TARGET_ROUGH_ROI_PADDING,
        TARGET_ROUGH_SKIP_PERSPECTIVE_ON_FAST_MOVE,
        TARGET_SMOOTHING_ALPHA_X100,
    )
except ImportError:
    CAMERA_WIDTH = 512
    CAMERA_HEIGHT = 320
    CAMERA_BUFFER_NUM = 1
    CAMERA_SKIP_FRAMES = 5
    CAMERA_CONTRAST = -1
    CAMERA_EXPOSURE = -1
    CAMERA_GAIN = -1
    CAMERA_WB_GAIN = []
    SHOW_FPS = True
    PRINT_FPS = False
    SHOW_CENTER_GUIDE = True
    SHOW_GRID = False
    SHOW_ROI = False
    SHOW_STATUS_TEXT = False
    SHOW_TARGET_BOX = True
    CROSSHAIR_SIZE = 24
    GRID_LINE_WIDTH = 1
    ENABLE_LASER_DETECT = False
    LASER_COLOR = "red"
    LASER_RED_THRESHOLDS = [[70, 100, 35, 127, -20, 127]]
    LASER_GREEN_THRESHOLDS = [[40, 100, -128, -15, -20, 80]]
    LASER_USE_ROI = True
    LASER_AREA_MIN = 2
    LASER_AREA_MAX = 120
    LASER_PIXELS_MIN = 2
    LASER_MIN_W = 1
    LASER_MIN_H = 1
    LASER_MAX_W = 30
    LASER_MAX_H = 30
    LASER_MAX_ASPECT_X100 = 300
    LASER_CONFIRM_FRAMES = 3
    LASER_CONFIRM_DISTANCE = 12
    PRINT_LASER = False
    ROI_SCALE_NUM = 4
    ROI_SCALE_DEN = 5
    TARGET_MARKER_BOX_MIN_W = 72
    TARGET_MARKER_BOX_MIN_H = 48
    TARGET_MARKER_BOX_PADDING = 14
    TARGET_MARKER_BOX_MAX_W = 220
    TARGET_MARKER_BOX_MAX_H = 170
    ENABLE_TARGET_DETECT = True
    TARGET_MODE = "perspective"
    TARGET_PERSPECTIVE_FALLBACK_BLOB = True
    TARGET_PERSPECTIVE_MIN_W = 40
    TARGET_PERSPECTIVE_MIN_H = 30
    TARGET_PERSPECTIVE_MIN_AREA = 1800
    TARGET_PERSPECTIVE_MAX_ASPECT_X100 = 450
    TARGET_PERSPECTIVE_DISTANCE_WEIGHT = 3
    TARGET_FAST_ROI_ENABLE = True
    TARGET_FAST_ROI_PADDING = 180
    TARGET_FULL_SCAN_INTERVAL = 2
    TARGET_JUMP_REJECT_ENABLE = False
    TARGET_MAX_CENTER_JUMP = 180
    TARGET_ROUGH_FIRST_ENABLE = True
    TARGET_ROUGH_ROI_PADDING = 140
    TARGET_ROUGH_FAST_MOVE_DISTANCE = 30
    TARGET_ROUGH_SKIP_PERSPECTIVE_ON_FAST_MOVE = False
    TARGET_ROUGH_OUTPUT_ENABLE = False
    TARGET_BLOB_CENTER_METHOD = "rect"
    TARGET_BLOB_THRESHOLDS = [[0, 45, -128, 127, -128, 127]]
    TARGET_BLOB_AREA_MIN = 80
    TARGET_BLOB_PIXELS_MIN = 80
    TARGET_BLOB_MIN_W = 6
    TARGET_BLOB_MIN_H = 6
    TARGET_BLOB_MAX_ASPECT_X100 = 350
    TARGET_SMOOTHING_ALPHA_X100 = 85
    TARGET_LOST_HOLD_FRAMES = 1
    TARGET_CIRCLE_THRESHOLD = 3000
    TARGET_RECT_THRESHOLD = 10000
    TARGET_MIN_RADIUS = 8
    TARGET_MAX_RADIUS = 110
    TARGET_MIN_RECT_W = 20
    TARGET_MIN_RECT_H = 20
    DETECT_EVERY_N_FRAMES = 1
    PRINT_TARGET = False


STAGE_NAME = "TARGET_DETECT"
FRAME_INDEX = 0
LAST_TARGET = None
SMOOTHED_TARGET = None
TARGET_LOST_COUNT = 0
LAST_LASER = None
LASER_CANDIDATE = None
LASER_CONFIRM_COUNT = 0


def frame_center():
    return CAMERA_WIDTH // 2, CAMERA_HEIGHT // 2


def create_camera():
    try:
        return camera.Camera(CAMERA_WIDTH, CAMERA_HEIGHT, buff_num=CAMERA_BUFFER_NUM)
    except TypeError:
        print("camera buff_num not supported, fallback to default buffer")
        return camera.Camera(CAMERA_WIDTH, CAMERA_HEIGHT)


def camera_setting_enabled(value):
    return value is not None and value >= 0


def call_camera_methods(cam, method_names, value):
    for method_name in method_names:
        try:
            method = getattr(cam, method_name)
        except Exception:
            continue

        try:
            method(value)
            return True
        except Exception as err:
            print("camera %s failed: %s" % (method_name, err))

    return False


def apply_camera_tuning(cam):
    if camera_setting_enabled(CAMERA_EXPOSURE):
        call_camera_methods(cam, ["exposure"], CAMERA_EXPOSURE)
    if camera_setting_enabled(CAMERA_GAIN):
        call_camera_methods(cam, ["gain"], CAMERA_GAIN)
    if camera_setting_enabled(CAMERA_CONTRAST):
        call_camera_methods(cam, ["constrast", "contrast"], CAMERA_CONTRAST)

    if CAMERA_WB_GAIN:
        try:
            cam.awb_mode(camera.AwbMode.Manual)
        except Exception as err:
            print("camera manual awb failed: %s" % err)
        call_camera_methods(cam, ["set_wb_gain"], CAMERA_WB_GAIN)


def skip_camera_startup_frames(cam):
    if CAMERA_SKIP_FRAMES <= 0:
        return

    try:
        cam.skip_frames(CAMERA_SKIP_FRAMES)
        return
    except Exception:
        pass

    for _ in range(CAMERA_SKIP_FRAMES):
        try:
            cam.read()
        except Exception:
            return


def get_roi():
    roi_w = CAMERA_WIDTH * ROI_SCALE_NUM // ROI_SCALE_DEN
    roi_h = CAMERA_HEIGHT * ROI_SCALE_NUM // ROI_SCALE_DEN
    roi_x = (CAMERA_WIDTH - roi_w) // 2
    roi_y = (CAMERA_HEIGHT - roi_h) // 2
    return roi_x, roi_y, roi_w, roi_h


def clamp_roi(x, y, w, h):
    left = max(0, x)
    top = max(0, y)
    right = min(CAMERA_WIDTH, x + w)
    bottom = min(CAMERA_HEIGHT, y + h)
    clipped_w = right - left
    clipped_h = bottom - top
    if clipped_w <= 0 or clipped_h <= 0:
        return get_roi()
    return [left, top, clipped_w, clipped_h]


def expanded_rect_roi(rect, padding):
    return clamp_roi(
        rect[0] - padding,
        rect[1] - padding,
        rect[2] + padding * 2,
        rect[3] + padding * 2,
    )


def should_full_scan():
    return TARGET_FULL_SCAN_INTERVAL > 0 and FRAME_INDEX % TARGET_FULL_SCAN_INTERVAL == 0


def get_search_roi():
    if not TARGET_FAST_ROI_ENABLE or should_full_scan():
        return get_roi()

    if not SMOOTHED_TARGET:
        return get_roi()
    if "rect" not in SMOOTHED_TARGET:
        return get_roi()

    return expanded_rect_roi(SMOOTHED_TARGET["rect"], TARGET_FAST_ROI_PADDING)


def point_in_roi(x, y):
    roi_x, roi_y, roi_w, roi_h = get_roi()
    return roi_x <= x <= roi_x + roi_w and roi_y <= y <= roi_y + roi_h


def safe_magnitude(obj):
    try:
        return obj.magnitude()
    except Exception:
        return 0


def safe_find_circles(img):
    roi = get_roi()
    try:
        return img.find_circles(roi=roi, threshold=TARGET_CIRCLE_THRESHOLD)
    except TypeError:
        return img.find_circles(threshold=TARGET_CIRCLE_THRESHOLD)


def safe_find_rects_in_roi(img, roi):
    try:
        return img.find_rects(roi=roi, threshold=TARGET_RECT_THRESHOLD)
    except TypeError:
        return img.find_rects(threshold=TARGET_RECT_THRESHOLD)


def safe_find_rects(img):
    return safe_find_rects_in_roi(img, get_search_roi())


def safe_find_blobs(img):
    roi = get_roi()
    try:
        return img.find_blobs(
            TARGET_BLOB_THRESHOLDS,
            roi=roi,
            area_threshold=TARGET_BLOB_AREA_MIN,
            pixels_threshold=TARGET_BLOB_PIXELS_MIN,
        )
    except TypeError:
        return img.find_blobs(
            TARGET_BLOB_THRESHOLDS,
            area_threshold=TARGET_BLOB_AREA_MIN,
            pixels_threshold=TARGET_BLOB_PIXELS_MIN,
        )


def laser_thresholds():
    if LASER_COLOR == "green":
        return LASER_GREEN_THRESHOLDS
    return LASER_RED_THRESHOLDS


def safe_find_laser_blobs(img):
    roi = get_roi()
    thresholds = laser_thresholds()
    try:
        if LASER_USE_ROI:
            return img.find_blobs(
                thresholds,
                roi=roi,
                area_threshold=LASER_AREA_MIN,
                pixels_threshold=LASER_PIXELS_MIN,
            )
        return img.find_blobs(
            thresholds,
            area_threshold=LASER_AREA_MIN,
            pixels_threshold=LASER_PIXELS_MIN,
        )
    except TypeError:
        return img.find_blobs(
            thresholds,
            area_threshold=LASER_AREA_MIN,
            pixels_threshold=LASER_PIXELS_MIN,
        )


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


def rect_area(rect):
    return rect[2] * rect[3]


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


def detect_blobs(img):
    try:
        blobs = safe_find_blobs(img)
    except MemoryError as err:
        print("find_blobs memory low: %s" % err)
        return None
    except Exception as err:
        print("find_blobs failed: %s" % err)
        return None

    center_x, center_y = target_anchor_center()
    best = None
    best_score = -1
    for blob in blobs:
        rect = blob_rect(blob)
        x, y = blob_center(blob, rect)
        w = rect[2]
        h = rect[3]
        if not point_in_roi(x, y):
            continue
        if w < TARGET_BLOB_MIN_W or h < TARGET_BLOB_MIN_H:
            continue

        long_side = max(w, h)
        short_side = max(1, min(w, h))
        aspect_x100 = long_side * 100 // short_side
        if aspect_x100 > TARGET_BLOB_MAX_ASPECT_X100:
            continue

        area = rect_area(rect)
        distance_penalty = abs(x - center_x) + abs(y - center_y)
        score = area - distance_penalty
        if score > best_score:
            best_score = score
            best = {
                "found": True,
                "type": "blob",
                "x": x,
                "y": y,
                "rect": rect,
                "score": score,
            }

    return best


def detect_laser(img):
    if not ENABLE_LASER_DETECT:
        return None

    try:
        blobs = safe_find_laser_blobs(img)
    except MemoryError as err:
        print("find_laser memory low: %s" % err)
        return None
    except Exception as err:
        print("find_laser failed: %s" % err)
        return None

    best = None
    best_score = -1
    for blob in blobs:
        rect = blob_rect(blob)
        x, y = rect_center(rect)
        w = rect[2]
        h = rect[3]
        area = rect_area(rect)
        if LASER_USE_ROI and not point_in_roi(x, y):
            continue
        if area < LASER_AREA_MIN or area > LASER_AREA_MAX:
            continue
        if w < LASER_MIN_W or h < LASER_MIN_H:
            continue
        if w > LASER_MAX_W or h > LASER_MAX_H:
            continue

        long_side = max(w, h)
        short_side = max(1, min(w, h))
        aspect_x100 = long_side * 100 // short_side
        if aspect_x100 > LASER_MAX_ASPECT_X100:
            continue

        score = area * 10 - aspect_x100
        if score > best_score:
            best_score = score
            best = {
                "found": True,
                "type": "laser",
                "x": x,
                "y": y,
                "rect": rect,
                "score": score,
            }

    return best


def laser_candidate_matches(candidate, raw_laser):
    if not candidate or not raw_laser:
        return False

    return (
        abs(candidate["x"] - raw_laser["x"]) <= LASER_CONFIRM_DISTANCE
        and abs(candidate["y"] - raw_laser["y"]) <= LASER_CONFIRM_DISTANCE
    )


def update_laser_tracking(raw_laser):
    global LASER_CANDIDATE, LASER_CONFIRM_COUNT

    if not ENABLE_LASER_DETECT or not raw_laser:
        LASER_CANDIDATE = None
        LASER_CONFIRM_COUNT = 0
        return None

    if LASER_CONFIRM_FRAMES <= 1:
        LASER_CANDIDATE = raw_laser
        LASER_CONFIRM_COUNT = 1
        return raw_laser

    if laser_candidate_matches(LASER_CANDIDATE, raw_laser):
        LASER_CONFIRM_COUNT += 1
    else:
        LASER_CANDIDATE = raw_laser
        LASER_CONFIRM_COUNT = 1

    if LASER_CONFIRM_COUNT >= LASER_CONFIRM_FRAMES:
        return raw_laser
    return None


def detect_circles(img):
    try:
        circles = safe_find_circles(img)
    except MemoryError as err:
        print("find_circles memory low: %s" % err)
        return None
    except Exception as err:
        print("find_circles failed: %s" % err)
        return None

    best = None
    best_score = -1
    for circle in circles:
        x = circle.x()
        y = circle.y()
        radius = circle.r()
        if not point_in_roi(x, y):
            continue
        if radius < TARGET_MIN_RADIUS or radius > TARGET_MAX_RADIUS:
            continue

        score = safe_magnitude(circle) + radius * 10
        if score > best_score:
            best_score = score
            best = {
                "found": True,
                "type": "circle",
                "x": x,
                "y": y,
                "radius": radius,
                "score": score,
            }

    return best


def same_roi(a, b):
    return a[0] == b[0] and a[1] == b[1] and a[2] == b[2] and a[3] == b[3]


def target_anchor_center(anchor_target=None):
    if anchor_target:
        return anchor_target["x"], anchor_target["y"]
    if SMOOTHED_TARGET:
        return SMOOTHED_TARGET["x"], SMOOTHED_TARGET["y"]
    return frame_center()


def append_unique_roi(rois, roi):
    for existing in rois:
        if same_roi(existing, roi):
            return
    rois.append(roi)


def select_perspective_target(rects, anchor_target=None):
    anchor_x, anchor_y = target_anchor_center(anchor_target)
    best = None
    best_score = -1
    for rect_obj in rects:
        corners = safe_rect_corners(rect_obj)
        if not corners:
            continue

        x, y = diagonal_center(corners)
        if not point_in_roi(x, y):
            continue

        rect = quad_bounds(corners)
        w = rect[2]
        h = rect[3]
        if w < TARGET_PERSPECTIVE_MIN_W or h < TARGET_PERSPECTIVE_MIN_H:
            continue

        long_side = max(w, h)
        short_side = max(1, min(w, h))
        aspect_x100 = long_side * 100 // short_side
        if aspect_x100 > TARGET_PERSPECTIVE_MAX_ASPECT_X100:
            continue

        area = quad_area(corners)
        if area < TARGET_PERSPECTIVE_MIN_AREA:
            continue

        distance_penalty = abs(x - anchor_x) + abs(y - anchor_y)
        score = area + safe_magnitude(rect_obj) - distance_penalty * TARGET_PERSPECTIVE_DISTANCE_WEIGHT
        if score > best_score:
            best_score = score
            best = {
                "found": True,
                "type": "perspective",
                "x": x,
                "y": y,
                "rect": rect,
                "corners": corners,
                "score": score,
            }

    return best


def detect_perspective_rects(img, rough_target=None):
    full_roi = get_roi()
    rois = []
    append_unique_roi(rois, get_search_roi())

    if rough_target and "rect" in rough_target:
        append_unique_roi(rois, expanded_rect_roi(rough_target["rect"], TARGET_ROUGH_ROI_PADDING))

    if should_full_scan() or not rough_target:
        append_unique_roi(rois, full_roi)

    for roi in rois:
        try:
            rects = safe_find_rects_in_roi(img, roi)
        except MemoryError as err:
            print("find_perspective memory low: %s" % err)
            return None
        except Exception as err:
            print("find_perspective failed: %s" % err)
            return None

        best = select_perspective_target(rects, rough_target)
        if best:
            return best

    return None


def detect_rects(img):
    try:
        rects = safe_find_rects(img)
    except MemoryError as err:
        print("find_rects memory low: %s" % err)
        return None
    except Exception as err:
        print("find_rects failed: %s" % err)
        return None

    best = None
    best_score = -1
    for rect_obj in rects:
        rect = rect_obj.rect()
        x = rect[0] + rect[2] // 2
        y = rect[1] + rect[3] // 2
        w = rect[2]
        h = rect[3]
        if not point_in_roi(x, y):
            continue
        if w < TARGET_MIN_RECT_W or h < TARGET_MIN_RECT_H:
            continue

        score = safe_magnitude(rect_obj) + w * h
        if score > best_score:
            best_score = score
            best = {
                "found": True,
                "type": "rect",
                "x": x,
                "y": y,
                "rect": rect,
                "corners": rect_obj.corners(),
                "score": score,
            }

    return best


def should_use_rough_target_now(rough_target):
    if not TARGET_ROUGH_SKIP_PERSPECTIVE_ON_FAST_MOVE:
        return False
    if not rough_target or not SMOOTHED_TARGET:
        return False

    dx = abs(rough_target["x"] - SMOOTHED_TARGET["x"])
    dy = abs(rough_target["y"] - SMOOTHED_TARGET["y"])
    return dx > TARGET_ROUGH_FAST_MOVE_DISTANCE or dy > TARGET_ROUGH_FAST_MOVE_DISTANCE


def detect_target(img):
    if not ENABLE_TARGET_DETECT:
        return None

    mode = TARGET_MODE
    perspective_target = None
    blob_target = None
    circle_target = None
    rect_target = None

    if mode == "perspective" or mode == "auto":
        if TARGET_ROUGH_FIRST_ENABLE:
            blob_target = detect_blobs(img)
            if TARGET_ROUGH_OUTPUT_ENABLE and blob_target and should_use_rough_target_now(blob_target):
                blob_target["type"] = "blob-fast"
                return blob_target
        perspective_target = detect_perspective_rects(img, blob_target)
    if mode == "blob" or mode == "auto":
        if not blob_target:
            blob_target = detect_blobs(img)
    if mode == "circle" or mode == "auto":
        circle_target = detect_circles(img)
    if mode == "rect" or mode == "auto":
        rect_target = detect_rects(img)

    if mode == "auto":
        best = None
        for target in (perspective_target, blob_target, circle_target, rect_target):
            if target and (best is None or target["score"] > best["score"]):
                best = target
        return best

    if perspective_target:
        return perspective_target
    if mode == "perspective" and TARGET_PERSPECTIVE_FALLBACK_BLOB:
        if not blob_target:
            blob_target = detect_blobs(img)
        if TARGET_ROUGH_OUTPUT_ENABLE and blob_target:
            blob_target["type"] = "blob-fast"
            return blob_target
    if mode != "perspective" and blob_target:
        return blob_target
    if circle_target:
        return circle_target
    if rect_target:
        return rect_target
    return None


def clone_target(target):
    if not target:
        return None

    cloned = {}
    for key in target:
        value = target[key]
        if key == "rect":
            cloned[key] = [value[0], value[1], value[2], value[3]]
        elif key == "corners":
            cloned[key] = [[point[0], point[1]] for point in value]
        else:
            cloned[key] = value
    cloned["raw_x"] = target["x"]
    cloned["raw_y"] = target["y"]
    cloned["stable"] = False
    return cloned


def smooth_value(old_value, new_value):
    alpha = TARGET_SMOOTHING_ALPHA_X100
    return (old_value * (100 - alpha) + new_value * alpha) // 100


def should_ignore_raw_target(raw_target):
    if not raw_target or not SMOOTHED_TARGET:
        return False

    old_type = SMOOTHED_TARGET.get("type")
    new_type = raw_target.get("type")

    if not TARGET_JUMP_REJECT_ENABLE:
        return False
    if old_type != "perspective" or new_type != "perspective":
        return False

    dx = abs(raw_target["x"] - SMOOTHED_TARGET["x"])
    dy = abs(raw_target["y"] - SMOOTHED_TARGET["y"])
    return dx > TARGET_MAX_CENTER_JUMP or dy > TARGET_MAX_CENTER_JUMP


def smooth_target_rect(target, smooth_x, smooth_y, raw_x, raw_y):
    dx = smooth_x - raw_x
    dy = smooth_y - raw_y

    if "rect" not in target:
        pass
    else:
        rect = target["rect"]
        rect[0] += dx
        rect[1] += dy

    if "corners" in target:
        for point in target["corners"]:
            point[0] += dx
            point[1] += dy


def update_target_tracking(raw_target):
    global SMOOTHED_TARGET, TARGET_LOST_COUNT

    if should_ignore_raw_target(raw_target):
        raw_target = None

    if raw_target:
        if SMOOTHED_TARGET and SMOOTHED_TARGET["type"] == raw_target["type"]:
            target = clone_target(raw_target)
            old_x = SMOOTHED_TARGET["x"]
            old_y = SMOOTHED_TARGET["y"]
            new_x = raw_target["x"]
            new_y = raw_target["y"]
            target["x"] = smooth_value(old_x, new_x)
            target["y"] = smooth_value(old_y, new_y)
            target["stable"] = True
            smooth_target_rect(target, target["x"], target["y"], new_x, new_y)
            SMOOTHED_TARGET = target
        else:
            SMOOTHED_TARGET = clone_target(raw_target)
        TARGET_LOST_COUNT = 0
        return SMOOTHED_TARGET

    if SMOOTHED_TARGET and TARGET_LOST_COUNT < TARGET_LOST_HOLD_FRAMES:
        TARGET_LOST_COUNT += 1
        SMOOTHED_TARGET["stable"] = True
        SMOOTHED_TARGET["lost_hold"] = TARGET_LOST_COUNT
        return SMOOTHED_TARGET

    TARGET_LOST_COUNT = TARGET_LOST_HOLD_FRAMES
    SMOOTHED_TARGET = None
    return None


def draw_center_guide(img):
    if SHOW_CENTER_GUIDE:
        center_x, center_y = frame_center()
        img.draw_line(
            center_x - CROSSHAIR_SIZE,
            center_y,
            center_x + CROSSHAIR_SIZE,
            center_y,
            image.COLOR_GREEN,
        )
        img.draw_line(
            center_x,
            center_y - CROSSHAIR_SIZE,
            center_x,
            center_y + CROSSHAIR_SIZE,
            image.COLOR_GREEN,
        )
        img.draw_circle(center_x, center_y, 4, image.COLOR_RED, 1)


def draw_grid(img):
    if not SHOW_GRID:
        return

    x1 = CAMERA_WIDTH // 3
    x2 = CAMERA_WIDTH * 2 // 3
    y1 = CAMERA_HEIGHT // 3
    y2 = CAMERA_HEIGHT * 2 // 3

    img.draw_line(x1, 0, x1, CAMERA_HEIGHT - 1, image.COLOR_BLUE, GRID_LINE_WIDTH)
    img.draw_line(x2, 0, x2, CAMERA_HEIGHT - 1, image.COLOR_BLUE, GRID_LINE_WIDTH)
    img.draw_line(0, y1, CAMERA_WIDTH - 1, y1, image.COLOR_BLUE, GRID_LINE_WIDTH)
    img.draw_line(0, y2, CAMERA_WIDTH - 1, y2, image.COLOR_BLUE, GRID_LINE_WIDTH)


def draw_roi(img):
    if not SHOW_ROI:
        return

    roi_x, roi_y, roi_w, roi_h = get_roi()
    img.draw_rect(roi_x, roi_y, roi_w, roi_h, image.COLOR_YELLOW, 2)
    img.draw_string(roi_x + 4, roi_y + 4, "ROI", image.COLOR_YELLOW)


def fps_state(fps):
    if fps >= 24:
        return "OK"
    if fps >= 15:
        return "MID"
    return "LOW"


def draw_status_text(img, fps):
    if not SHOW_STATUS_TEXT:
        return

    center_x, center_y = frame_center()
    roi_x, roi_y, roi_w, roi_h = get_roi()

    img.draw_string(8, 8, "TrackingCar Vision", image.COLOR_GREEN)
    img.draw_string(8, 28, "stage: %s" % STAGE_NAME, image.COLOR_GREEN)
    if SHOW_FPS:
        img.draw_string(8, 48, "FPS: %.1f %s" % (fps, fps_state(fps)), image.COLOR_GREEN)
    img.draw_string(8, 68, "size: %dx%d" % (CAMERA_WIDTH, CAMERA_HEIGHT), image.COLOR_GREEN)
    img.draw_string(8, 88, "center: (%d,%d)" % (center_x, center_y), image.COLOR_GREEN)
    img.draw_string(8, 108, "roi: (%d,%d,%d,%d)" % (roi_x, roi_y, roi_w, roi_h), image.COLOR_GREEN)
    img.draw_string(8, 128, "x->right  y->down", image.COLOR_GREEN)


def clip_rect_to_frame(x, y, w, h):
    left = max(0, x)
    top = max(0, y)
    right = min(CAMERA_WIDTH - 1, x + w)
    bottom = min(CAMERA_HEIGHT - 1, y + h)
    clipped_w = right - left
    clipped_h = bottom - top
    if clipped_w <= 0 or clipped_h <= 0:
        return None
    return [left, top, clipped_w, clipped_h]


def target_marker_box_rect(target, x, y):
    box_w = TARGET_MARKER_BOX_MIN_W
    box_h = TARGET_MARKER_BOX_MIN_H
    if "radius" in target:
        diameter = target["radius"] * 2
        box_w = max(box_w, diameter)
        box_h = max(box_h, diameter)
    if "rect" in target:
        rect = target["rect"]
        box_w = max(box_w, rect[2])
        box_h = max(box_h, rect[3])

    box_w = min(box_w + TARGET_MARKER_BOX_PADDING * 2, TARGET_MARKER_BOX_MAX_W)
    box_h = min(box_h + TARGET_MARKER_BOX_PADDING * 2, TARGET_MARKER_BOX_MAX_H)
    return clip_rect_to_frame(x - box_w // 2, y - box_h // 2, box_w, box_h)


def draw_stable_target_box(img, target, x, y):
    if not SHOW_TARGET_BOX:
        return

    rect = target_marker_box_rect(target, x, y)
    if rect:
        img.draw_rect(rect[0], rect[1], rect[2], rect[3], image.COLOR_RED, 2)


def draw_target_outline(img, target, x, y):
    if not SHOW_TARGET_BOX:
        return

    if "corners" in target:
        corners = target["corners"]
        for i in range(4):
            img.draw_line(
                corners[i][0],
                corners[i][1],
                corners[(i + 1) % 4][0],
                corners[(i + 1) % 4][1],
                image.COLOR_RED,
                2,
            )
        return

    draw_stable_target_box(img, target, x, y)


def draw_target_marker(img, target):
    if not target:
        return

    center_x, center_y = frame_center()
    x = target["x"]
    y = target["y"]

    draw_target_outline(img, target, x, y)

    img.draw_line(x - 12, y, x + 12, y, image.COLOR_RED, 2)
    img.draw_line(x, y - 12, x, y + 12, image.COLOR_RED, 2)
    img.draw_circle(x, y, 3, image.COLOR_RED, 2)
    img.draw_line(center_x, center_y, x, y, image.COLOR_RED, 1)
    if "raw_x" in target and (target["raw_x"] != x or target["raw_y"] != y):
        img.draw_circle(target["raw_x"], target["raw_y"], 4, image.COLOR_YELLOW, 1)


def draw_laser_marker(img, laser):
    if not laser:
        return

    x = laser["x"]
    y = laser["y"]
    rect = laser["rect"]
    img.draw_rect(rect[0], rect[1], rect[2], rect[3], image.COLOR_BLUE, 1)
    img.draw_line(x - 8, y, x + 8, y, image.COLOR_BLUE, 2)
    img.draw_line(x, y - 8, x, y + 8, image.COLOR_BLUE, 2)
    img.draw_circle(x, y, 4, image.COLOR_BLUE, 2)


def draw_aim_status(img, target, laser):
    y0 = CAMERA_HEIGHT - 64
    if target:
        img.draw_string(8, y0, "target: %s (%d,%d)" % (target["type"], target["x"], target["y"]), image.COLOR_RED)
    else:
        img.draw_string(8, y0, "target: LOST", image.COLOR_RED)

    if not ENABLE_LASER_DETECT:
        img.draw_string(8, y0 + 20, "laser: OFF", image.COLOR_BLUE)
    elif laser:
        img.draw_string(8, y0 + 20, "laser: %s (%d,%d)" % (LASER_COLOR, laser["x"], laser["y"]), image.COLOR_BLUE)
    else:
        img.draw_string(8, y0 + 20, "laser: LOST", image.COLOR_BLUE)

    if target and laser:
        dx = target["x"] - laser["x"]
        dy = target["y"] - laser["y"]
        img.draw_string(8, y0 + 40, "aim: dx=%d dy=%d" % (dx, dy), image.COLOR_YELLOW)
        img.draw_line(laser["x"], laser["y"], target["x"], target["y"], image.COLOR_YELLOW, 1)
    elif target and not ENABLE_LASER_DETECT:
        img.draw_string(8, y0 + 40, "aim: TARGET ONLY", image.COLOR_YELLOW)
    else:
        img.draw_string(8, y0 + 40, "aim: WAIT", image.COLOR_YELLOW)


def draw_debug_overlay(img, fps, target, laser):
    draw_grid(img)
    draw_roi(img)
    draw_target_marker(img, target)
    draw_laser_marker(img, laser)
    draw_center_guide(img)
    draw_status_text(img, fps)
    draw_aim_status(img, target, laser)


def process_frame(img, fps):
    """Detect the target and draw the debug view."""
    global FRAME_INDEX, LAST_TARGET, LAST_LASER

    FRAME_INDEX += 1
    if DETECT_EVERY_N_FRAMES <= 1 or FRAME_INDEX % DETECT_EVERY_N_FRAMES == 0:
        raw_target = detect_target(img)
        LAST_TARGET = update_target_tracking(raw_target)
        LAST_LASER = update_laser_tracking(detect_laser(img))

    target = LAST_TARGET
    laser = LAST_LASER
    draw_debug_overlay(img, fps, target, laser)
    if PRINT_TARGET and target:
        center_x, center_y = frame_center()
        print(
            "target=%s x=%d y=%d dx=%d dy=%d"
            % (target["type"], target["x"], target["y"], target["x"] - center_x, target["y"] - center_y)
        )
    if PRINT_LASER and laser:
        print("laser=%s x=%d y=%d" % (LASER_COLOR, laser["x"], laser["y"]))
    return img


def main():
    cam = create_camera()
    apply_camera_tuning(cam)
    skip_camera_startup_frames(cam)
    disp = display.Display()

    while not app.need_exit():
        img = cam.read()
        fps = time.fps()
        img = process_frame(img, fps)

        disp.show(img)

        if PRINT_FPS:
            print("fps: %.2f" % fps)


if __name__ == "__main__":
    main()
