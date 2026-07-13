from settings import *
from app import runtime_state as state
from vision.geometry import *


def target_search_roi():
    if not TARGET_USE_TRACKING_ROI:
        return get_roi()
    if not state.LAST_TARGET or "rect" not in state.LAST_TARGET:
        return get_roi()
    if "lost_hold" in state.LAST_TARGET:
        return get_roi()
    if TARGET_FULL_SEARCH_EVERY_N_FRAMES > 0 and state.FRAME_INDEX % TARGET_FULL_SEARCH_EVERY_N_FRAMES == 0:
        return get_roi()

    rect = state.LAST_TARGET["rect"]
    margin = TARGET_TRACKING_ROI_MARGIN
    return clamp_rect(
        rect[0] - margin,
        rect[1] - margin,
        rect[2] + margin * 2,
        rect[3] + margin * 2,
    )


def safe_find_circles(img):
    roi = target_search_roi()
    try:
        return img.find_circles(roi=roi, threshold=TARGET_CIRCLE_THRESHOLD)
    except TypeError:
        return img.find_circles(threshold=TARGET_CIRCLE_THRESHOLD)


def safe_find_rects(img, roi=None):
    if roi is None:
        roi = target_search_roi()
    try:
        return img.find_rects(roi=roi, threshold=TARGET_RECT_THRESHOLD)
    except TypeError:
        return img.find_rects(threshold=TARGET_RECT_THRESHOLD)


def safe_find_blobs(img, roi=None):
    if roi is None:
        roi = target_search_roi()
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


def find_largest_black_blob_roi(img, search_roi):
    if not TARGET_RECT_USE_BLOB_ROI:
        return None

    try:
        blobs = safe_find_blobs(img, search_roi)
    except MemoryError as err:
        print("find_target_blob_roi memory low: %s" % err)
        return None
    except Exception as err:
        print("find_target_blob_roi failed: %s" % err)
        return None

    best_rect = None
    best_score = -1
    for blob in blobs:
        rect = blob_rect(blob)
        x, y = blob_center(blob, rect)
        w = rect[2]
        h = rect[3]
        if search_roi and not point_in_rect(x, y, search_roi):
            continue
        if w < TARGET_BLOB_MIN_W or h < TARGET_BLOB_MIN_H:
            continue

        long_side = max(w, h)
        short_side = max(1, min(w, h))
        aspect_x100 = long_side * 100 // short_side
        if aspect_x100 > TARGET_BLOB_MAX_ASPECT_X100:
            continue

        pixels = blob_pixels(blob, rect_area(rect))
        if pixels > best_score:
            best_score = pixels
            best_rect = rect

    if not best_rect:
        return None

    margin = TARGET_RECT_BLOB_ROI_MARGIN
    return clamp_rect(
        best_rect[0] - margin,
        best_rect[1] - margin,
        best_rect[2] + margin * 2,
        best_rect[3] + margin * 2,
    )


def detect_blobs(img):
    try:
        blobs = safe_find_blobs(img)
    except MemoryError as err:
        print("find_blobs memory low: %s" % err)
        return None
    except Exception as err:
        print("find_blobs failed: %s" % err)
        return None

    center_x, center_y = frame_center()
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


def detect_perspective_rects(img):
    try:
        rects = safe_find_rects(img)
    except MemoryError as err:
        print("find_perspective memory low: %s" % err)
        return None
    except Exception as err:
        print("find_perspective failed: %s" % err)
        return None

    center_x, center_y = frame_center()
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

        distance_penalty = abs(x - center_x) + abs(y - center_y)
        score = area + safe_magnitude(rect_obj) - distance_penalty
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


def detect_target(img):
    if not ENABLE_TARGET_DETECT:
        return None

    mode = TARGET_MODE
    perspective_target = None
    blob_target = None
    circle_target = None
    rect_target = None

    if mode == "perspective" or mode == "auto":
        perspective_target = detect_perspective_rects(img)
    if mode == "blob" or mode == "auto":
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
        blob_target = detect_blobs(img)
        if blob_target:
            blob_target["type"] = "blob-fallback"
            return blob_target
    if blob_target:
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


def smooth_target_rect(target, smooth_x, smooth_y, raw_x, raw_y):
    dx = smooth_x - raw_x
    dy = smooth_y - raw_y

    if "corners" in target and state.SMOOTHED_TARGET and "corners" in state.SMOOTHED_TARGET:
        old_corners = state.SMOOTHED_TARGET["corners"]
        if len(old_corners) == len(target["corners"]):
            for index, point in enumerate(target["corners"]):
                point[0] = weighted_value(old_corners[index][0], point[0], TARGET_CORNER_SMOOTHING_ALPHA_X100)
                point[1] = weighted_value(old_corners[index][1], point[1], TARGET_CORNER_SMOOTHING_ALPHA_X100)
            target["rect"] = quad_bounds(target["corners"])
            target["x"], target["y"] = diagonal_center(target["corners"])
            return

    if "rect" in target:
        rect = target["rect"]
        if state.SMOOTHED_TARGET and "rect" in state.SMOOTHED_TARGET:
            old_rect = state.SMOOTHED_TARGET["rect"]
            rect[2] = weighted_value(old_rect[2], rect[2], TARGET_SIZE_SMOOTHING_ALPHA_X100)
            rect[3] = weighted_value(old_rect[3], rect[3], TARGET_SIZE_SMOOTHING_ALPHA_X100)
            rect[0] = smooth_x - rect[2] // 2
            rect[1] = smooth_y - rect[3] // 2
        else:
            rect[0] += dx
            rect[1] += dy

    if "corners" in target:
        for point in target["corners"]:
            point[0] += dx
            point[1] += dy


def target_can_smooth(raw_target):
    if not state.SMOOTHED_TARGET or not raw_target:
        return False
    if state.SMOOTHED_TARGET["type"] == raw_target["type"]:
        return True
    if TARGET_SMOOTH_MAX_JUMP <= 0:
        return False

    jump = distance_xy(state.SMOOTHED_TARGET["x"], state.SMOOTHED_TARGET["y"], raw_target["x"], raw_target["y"])
    return jump <= TARGET_SMOOTH_MAX_JUMP


def smooth_target(raw_target):
    target = clone_target(raw_target)
    old_x = state.SMOOTHED_TARGET["x"]
    old_y = state.SMOOTHED_TARGET["y"]
    new_x = raw_target["x"]
    new_y = raw_target["y"]
    target["x"] = smooth_value(old_x, new_x)
    target["y"] = smooth_value(old_y, new_y)
    target["stable"] = True
    smooth_target_rect(target, target["x"], target["y"], new_x, new_y)
    return target


def update_target_tracking(raw_target):
    if raw_target:
        if target_can_smooth(raw_target):
            state.SMOOTHED_TARGET = smooth_target(raw_target)
        else:
            state.SMOOTHED_TARGET = clone_target(raw_target)
        state.TARGET_LOST_COUNT = 0
        return state.SMOOTHED_TARGET

    if state.SMOOTHED_TARGET and state.TARGET_LOST_COUNT < TARGET_LOST_HOLD_FRAMES:
        state.TARGET_LOST_COUNT += 1
        state.SMOOTHED_TARGET["stable"] = True
        state.SMOOTHED_TARGET["lost_hold"] = state.TARGET_LOST_COUNT
        return state.SMOOTHED_TARGET

    state.TARGET_LOST_COUNT = TARGET_LOST_HOLD_FRAMES
    state.SMOOTHED_TARGET = None
    return None
