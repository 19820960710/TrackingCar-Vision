from maix import app, camera, display, image, time

try:
    from config import (
        CAMERA_HEIGHT,
        CAMERA_WIDTH,
        CROSSHAIR_SIZE,
        GRID_LINE_WIDTH,
        PRINT_FPS,
        ROI_SCALE_DEN,
        ROI_SCALE_NUM,
        SHOW_CENTER_GUIDE,
        SHOW_FPS,
        SHOW_GRID,
        SHOW_ROI,
        SHOW_STATUS_TEXT,
    )
except ImportError:
    CAMERA_WIDTH = 640
    CAMERA_HEIGHT = 480
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


STAGE_NAME = "DEBUG_VIEW"


def frame_center():
    return CAMERA_WIDTH // 2, CAMERA_HEIGHT // 2


def get_roi():
    roi_w = CAMERA_WIDTH * ROI_SCALE_NUM // ROI_SCALE_DEN
    roi_h = CAMERA_HEIGHT * ROI_SCALE_NUM // ROI_SCALE_DEN
    roi_x = (CAMERA_WIDTH - roi_w) // 2
    roi_y = (CAMERA_HEIGHT - roi_h) // 2
    return roi_x, roi_y, roi_w, roi_h


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


def draw_debug_overlay(img, fps):
    draw_grid(img)
    draw_roi(img)
    draw_center_guide(img)
    draw_status_text(img, fps)


def process_frame(img, fps):
    """Stage-1 debug view.

    Later stages will add target detection, laser spot detection, and serial
    output here. For now, draw the coordinate guide used by all later logic.
    """
    draw_debug_overlay(img, fps)
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
