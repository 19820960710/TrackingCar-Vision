from maix import app, camera, display, image, time

try:
    from config import (
        CAMERA_HEIGHT,
        CAMERA_WIDTH,
        CROSSHAIR_SIZE,
        DETECT_EVERY_N_FRAMES,
        ENABLE_TARGET_DETECT,
        GRID_LINE_WIDTH,
        PRINT_FPS,
        PRINT_TARGET,
        ROI_SCALE_DEN,
        ROI_SCALE_NUM,
        SHOW_CENTER_GUIDE,
        SHOW_FPS,
        SHOW_GRID,
        SHOW_ROI,
        SHOW_STATUS_TEXT,
        TARGET_CIRCLE_THRESHOLD,
        TARGET_MAX_RADIUS,
        TARGET_MIN_RADIUS,
        TARGET_MIN_RECT_H,
        TARGET_MIN_RECT_W,
        TARGET_MODE,
        TARGET_RECT_THRESHOLD,
    )
except ImportError:
    CAMERA_WIDTH = 320
    CAMERA_HEIGHT = 240
    SHOW_FPS = True
    PRINT_FPS = True
    SHOW_CENTER_GUIDE = True
    SHOW_GRID = True
    SHOW_ROI = True
    SHOW_STATUS_TEXT = True
    CROSSHAIR_SIZE = 24
    GRID_LINE_WIDTH = 1
    ROI_SCALE_NUM = 4
    ROI_SCALE_DEN = 5
    ENABLE_TARGET_DETECT = True
    TARGET_MODE = "circle"
    TARGET_CIRCLE_THRESHOLD = 3000
    TARGET_RECT_THRESHOLD = 10000
    TARGET_MIN_RADIUS = 8
    TARGET_MAX_RADIUS = 110
    TARGET_MIN_RECT_W = 20
    TARGET_MIN_RECT_H = 20
    DETECT_EVERY_N_FRAMES = 3
    PRINT_TARGET = False


STAGE_NAME = "TARGET_DETECT"
FRAME_INDEX = 0
LAST_TARGET = None


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
    circle_target = None
    rect_target = None

    if mode == "circle" or mode == "auto":
        circle_target = detect_circles(img)
    if mode == "rect" or mode == "auto":
        rect_target = detect_rects(img)

    if circle_target and rect_target:
        return circle_target if circle_target["score"] >= rect_target["score"] else rect_target
    if circle_target:
        return circle_target
    if rect_target:
        return rect_target
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
        img.draw_string(8, CAMERA_HEIGHT - 24, "target: LOST", image.COLOR_RED)
        return

    center_x, center_y = frame_center()
    x = target["x"]
    y = target["y"]
    dx = x - center_x
    dy = y - center_y

    if target["type"] == "circle":
        img.draw_circle(x, y, target["radius"], image.COLOR_RED, 2)
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
    img.draw_string(8, CAMERA_HEIGHT - 44, "target: %s (%d,%d)" % (target["type"], x, y), image.COLOR_RED)
    img.draw_string(8, CAMERA_HEIGHT - 24, "offset: dx=%d dy=%d" % (dx, dy), image.COLOR_RED)


def draw_debug_overlay(img, fps, target):
    draw_grid(img)
    draw_roi(img)
    draw_target_marker(img, target)
    draw_center_guide(img)
    draw_status_text(img, fps)


def process_frame(img, fps):
    """Detect the target and draw the debug view."""
    global FRAME_INDEX, LAST_TARGET

    FRAME_INDEX += 1
    if DETECT_EVERY_N_FRAMES <= 1 or FRAME_INDEX % DETECT_EVERY_N_FRAMES == 0:
        LAST_TARGET = detect_target(img)

    target = LAST_TARGET
    draw_debug_overlay(img, fps, target)
    if PRINT_TARGET and target:
        center_x, center_y = frame_center()
        print(
            "target=%s x=%d y=%d dx=%d dy=%d"
            % (target["type"], target["x"], target["y"], target["x"] - center_x, target["y"] - center_y)
        )
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
