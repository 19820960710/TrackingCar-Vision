from maix import app, camera, display, image, time

try:
    from config import (
        CAMERA_HEIGHT,
        CAMERA_WIDTH,
        CROSSHAIR_SIZE,
        DETECT_EVERY_N_FRAMES,
        ENABLE_TARGET_DETECT,
        GRID_LINE_WIDTH,
        ENABLE_LASER_DETECT,
        LASER_AREA_MAX,
        LASER_AREA_MIN,
        LASER_COLOR,
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
        TARGET_CIRCLE_THRESHOLD,
        TARGET_BLOB_AREA_MIN,
        TARGET_BLOB_MAX_ASPECT_X100,
        TARGET_BLOB_MIN_H,
        TARGET_BLOB_MIN_W,
        TARGET_BLOB_PIXELS_MIN,
        TARGET_BLOB_THRESHOLDS,
        TARGET_LOST_HOLD_FRAMES,
        TARGET_MAX_RADIUS,
        TARGET_MIN_RADIUS,
        TARGET_MIN_RECT_H,
        TARGET_MIN_RECT_W,
        TARGET_MODE,
        TARGET_RECT_THRESHOLD,
        TARGET_SMOOTHING_ALPHA_X100,
    )
except ImportError:
    CAMERA_WIDTH = 512
    CAMERA_HEIGHT = 320
    SHOW_FPS = True
    PRINT_FPS = True
    SHOW_CENTER_GUIDE = True
    SHOW_GRID = True
    SHOW_ROI = True
    SHOW_STATUS_TEXT = True
    CROSSHAIR_SIZE = 24
    GRID_LINE_WIDTH = 1
    ENABLE_LASER_DETECT = True
    LASER_COLOR = "red"
    LASER_RED_THRESHOLDS = [[40, 100, 30, 127, -20, 127]]
    LASER_GREEN_THRESHOLDS = [[40, 100, -128, -15, -20, 80]]
    LASER_USE_ROI = True
    LASER_AREA_MIN = 2
    LASER_AREA_MAX = 250
    LASER_PIXELS_MIN = 2
    LASER_MIN_W = 1
    LASER_MIN_H = 1
    LASER_MAX_W = 30
    LASER_MAX_H = 30
    LASER_MAX_ASPECT_X100 = 300
    PRINT_LASER = False
    ROI_SCALE_NUM = 4
    ROI_SCALE_DEN = 5
    ENABLE_TARGET_DETECT = True
    TARGET_MODE = "blob"
    TARGET_BLOB_THRESHOLDS = [[0, 45, -128, 127, -128, 127]]
    TARGET_BLOB_AREA_MIN = 80
    TARGET_BLOB_PIXELS_MIN = 80
    TARGET_BLOB_MIN_W = 6
    TARGET_BLOB_MIN_H = 6
    TARGET_BLOB_MAX_ASPECT_X100 = 350
    TARGET_SMOOTHING_ALPHA_X100 = 35
    TARGET_LOST_HOLD_FRAMES = 5
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


def safe_find_rects(img):
    roi = get_roi()
    try:
        return img.find_rects(roi=roi, threshold=TARGET_RECT_THRESHOLD)
    except TypeError:
        return img.find_rects(threshold=TARGET_RECT_THRESHOLD)


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


def blob_corners(blob):
    try:
        corners = blob.corners()
        if corners and len(corners) >= 4:
            return [
                [corners[0][0], corners[0][1]],
                [corners[1][0], corners[1][1]],
                [corners[2][0], corners[2][1]],
                [corners[3][0], corners[3][1]],
            ]
    except Exception:
        pass

    return None


def rect_center(rect):
    return rect[0] + rect[2] // 2, rect[1] + rect[3] // 2


def corners_center(corners):
    x_sum = 0
    y_sum = 0
    for corner in corners:
        x_sum += corner[0]
        y_sum += corner[1]
    return x_sum // len(corners), y_sum // len(corners)


def rect_area(rect):
    return rect[2] * rect[3]


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
        corners = blob_corners(blob)
        if corners:
            x, y = corners_center(corners)
        else:
            x, y = rect_center(rect)
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
                "corners": corners,
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
    blob_target = None
    circle_target = None
    rect_target = None

    if mode == "blob" or mode == "auto":
        blob_target = detect_blobs(img)
    if mode == "circle" or mode == "auto":
        circle_target = detect_circles(img)
    if mode == "rect" or mode == "auto":
        rect_target = detect_rects(img)

    if mode == "auto":
        best = None
        for target in (blob_target, circle_target, rect_target):
            if target and (best is None or target["score"] > best["score"]):
                best = target
        return best

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
        elif key == "corners" and value:
            cloned[key] = [
                [value[0][0], value[0][1]],
                [value[1][0], value[1][1]],
                [value[2][0], value[2][1]],
                [value[3][0], value[3][1]],
            ]
        else:
            cloned[key] = value
    cloned["raw_x"] = target["x"]
    cloned["raw_y"] = target["y"]
    cloned["stable"] = False
    return cloned


def smooth_value(old_value, new_value):
    alpha = TARGET_SMOOTHING_ALPHA_X100
    return (old_value * (100 - alpha) + new_value * alpha) // 100


def smooth_target_rect(target, smooth_x, smooth_y, raw_x, raw_y):
    dx = smooth_x - raw_x
    dy = smooth_y - raw_y

    if "rect" not in target:
        pass
    else:
        rect = target["rect"]
        rect[0] += dx
        rect[1] += dy

    if "corners" in target and target["corners"]:
        for corner in target["corners"]:
            corner[0] += dx
            corner[1] += dy


def update_target_tracking(raw_target):
    global SMOOTHED_TARGET, TARGET_LOST_COUNT

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


def draw_target_marker(img, target):
    if not target:
        return

    center_x, center_y = frame_center()
    x = target["x"]
    y = target["y"]
    dx = x - center_x
    dy = y - center_y

    if target["type"] == "circle":
        img.draw_circle(x, y, target["radius"], image.COLOR_RED, 2)
    elif target["type"] == "blob":
        corners = target.get("corners", None)
        if corners:
            for i in range(4):
                img.draw_line(
                    corners[i][0],
                    corners[i][1],
                    corners[(i + 1) % 4][0],
                    corners[(i + 1) % 4][1],
                    image.COLOR_RED,
                    2,
                )
        else:
            rect = target["rect"]
            img.draw_rect(rect[0], rect[1], rect[2], rect[3], image.COLOR_RED, 2)
    elif target["type"] == "rect":
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

    if laser:
        img.draw_string(8, y0 + 20, "laser: %s (%d,%d)" % (LASER_COLOR, laser["x"], laser["y"]), image.COLOR_BLUE)
    else:
        img.draw_string(8, y0 + 20, "laser: LOST", image.COLOR_BLUE)

    if target and laser:
        dx = target["x"] - laser["x"]
        dy = target["y"] - laser["y"]
        img.draw_string(8, y0 + 40, "aim: dx=%d dy=%d" % (dx, dy), image.COLOR_YELLOW)
        img.draw_line(laser["x"], laser["y"], target["x"], target["y"], image.COLOR_YELLOW, 1)
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
        LAST_LASER = detect_laser(img)

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
    cam = camera.Camera(CAMERA_WIDTH, CAMERA_HEIGHT)
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
