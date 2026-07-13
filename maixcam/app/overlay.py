from maix import image

from settings import *
from app import runtime_state as state
from vision.geometry import frame_center, get_roi
from vision.laser import laser_calibrating, laser_calibration_status
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

    if SHOW_FPS:
        img.draw_string(8, 8, "FPS: %.1f %s" % (fps, fps_state(fps)), image.COLOR_GREEN)
    if not SHOW_VERBOSE_STATUS:
        return

    center_x, center_y = frame_center()
    roi_x, roi_y, roi_w, roi_h = get_roi()
    img.draw_string(8, 28, "stage: %s" % state.STAGE_NAME, image.COLOR_GREEN)
    img.draw_string(8, 48, "size: %dx%d" % (CAMERA_WIDTH, CAMERA_HEIGHT), image.COLOR_GREEN)
    img.draw_string(8, 68, "center: (%d,%d)" % (center_x, center_y), image.COLOR_GREEN)
    img.draw_string(8, 88, "roi: (%d,%d,%d,%d)" % (roi_x, roi_y, roi_w, roi_h), image.COLOR_GREEN)
    img.draw_string(8, 108, "cfg: %s %s" % (CONFIG_SOURCE, CONFIG_VERSION), image.COLOR_GREEN)


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
        freeze_text = ""
        if state.TARGET_FREEZE_COUNT > 0:
            freeze_text = " FZ%d" % state.TARGET_FREEZE_COUNT
        img.draw_string(
            8,
            y0,
            "target: %s (%d,%d)%s" % (target["type"], target["x"], target["y"], freeze_text),
            image.COLOR_RED,
        )
    else:
        img.draw_string(8, y0, "target: LOST", image.COLOR_RED)

    if not ENABLE_LASER_DETECT:
        img.draw_string(8, y0 + 20, "laser: OFF", image.COLOR_BLUE)
    elif laser_calibrating():
        phase, count, total = laser_calibration_status()
        img.draw_string(
            8,
            y0 + 20,
            "laser: CAL %s %d/%d KEEP OFF" % (phase, count, total),
            image.COLOR_BLUE,
        )
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
