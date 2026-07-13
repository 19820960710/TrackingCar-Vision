from maix import time

from settings import *
from app import runtime_state as state
from app.overlay import draw_debug_overlay
from drivers.uart_output import send_uart_result
from vision.geometry import frame_center
from vision.laser import detect_laser, laser_calibrating, update_laser_tracking
from vision.target import detect_target, update_target_tracking
def time_ticks_ms():
    try:
        return time.ticks_ms()
    except Exception:
        return None


def elapsed_ms(start_ms):
    if start_ms is None:
        return -1

    now_ms = time_ticks_ms()
    if now_ms is None:
        return -1

    try:
        return time.ticks_diff(now_ms, start_ms)
    except Exception:
        return now_ms - start_ms


def print_timing_if_due(target_ms, laser_ms, total_ms):
    if not PRINT_TIMING:
        return
    if TIMING_PRINT_EVERY_N_FRAMES <= 0:
        return
    if state.FRAME_INDEX % TIMING_PRINT_EVERY_N_FRAMES != 0:
        return

    print("timing target=%dms laser=%dms total=%dms" % (target_ms, laser_ms, total_ms))


def process_frame(img, fps):
    """Detect the target and draw the debug view."""
    total_start_ms = time_ticks_ms()
    target_ms = -1
    laser_ms = -1
    state.FRAME_INDEX += 1

    laser_first = state.LAST_TARGET is not None
    if laser_first:
        laser_start_ms = time_ticks_ms()
        raw_laser = detect_laser(img)
        if raw_laser and TARGET_FREEZE_HOLD_FRAMES_AFTER_LASER > 0:
            state.TARGET_FREEZE_COUNT = TARGET_FREEZE_HOLD_FRAMES_AFTER_LASER
        elif state.TARGET_FREEZE_COUNT > 0:
            state.TARGET_FREEZE_COUNT -= 1
        state.LAST_LASER = update_laser_tracking(raw_laser)
        laser_ms = elapsed_ms(laser_start_ms)

    active_laser = state.LAST_LASER and "lost_hold" not in state.LAST_LASER and not laser_calibrating()
    recent_laser = state.TARGET_FREEZE_COUNT > 0 or (state.LAST_LASER is not None and not laser_calibrating())
    target_interval = DETECT_EVERY_N_FRAMES
    if state.LAST_TARGET and recent_laser and TARGET_DETECT_EVERY_N_FRAMES_WHEN_LASER > target_interval:
        target_interval = TARGET_DETECT_EVERY_N_FRAMES_WHEN_LASER
    target_interval_due = (
        target_interval <= 1
        or state.FRAME_INDEX == 1
        or state.FRAME_INDEX % target_interval == 0
    )
    target_frozen = state.TARGET_FREEZE_COUNT > 0 and TARGET_FREEZE_WHEN_LASER
    target_due = not state.LAST_TARGET or (not target_frozen and target_interval_due)

    if target_due:
        target_start_ms = time_ticks_ms()
        raw_target = detect_target(img)
        target_ms = elapsed_ms(target_start_ms)
        state.LAST_TARGET = update_target_tracking(raw_target)

    if not laser_first:
        laser_start_ms = time_ticks_ms()
        raw_laser = detect_laser(img)
        if raw_laser and TARGET_FREEZE_HOLD_FRAMES_AFTER_LASER > 0:
            state.TARGET_FREEZE_COUNT = TARGET_FREEZE_HOLD_FRAMES_AFTER_LASER
        elif state.TARGET_FREEZE_COUNT > 0:
            state.TARGET_FREEZE_COUNT -= 1
        state.LAST_LASER = update_laser_tracking(raw_laser)
        laser_ms = elapsed_ms(laser_start_ms)

    target = state.LAST_TARGET
    laser = state.LAST_LASER
    send_uart_result(target, laser)
    draw_debug_overlay(img, fps, target, laser)
    if PRINT_TARGET and target:
        center_x, center_y = frame_center()
        print(
            "target=%s x=%d y=%d dx=%d dy=%d"
            % (target["type"], target["x"], target["y"], target["x"] - center_x, target["y"] - center_y)
        )
    if PRINT_LASER and laser:
        print("laser=%s x=%d y=%d" % (LASER_COLOR, laser["x"], laser["y"]))
    print_timing_if_due(target_ms, laser_ms, elapsed_ms(total_start_ms))
    return img


def print_startup_config():
    print("TrackingCar Vision config: %s %s" % (CONFIG_SOURCE, CONFIG_VERSION))
    print("camera: %dx%d @ %d fps, detect every %d frame(s)" % (
        CAMERA_WIDTH,
        CAMERA_HEIGHT,
        CAMERA_FPS,
        DETECT_EVERY_N_FRAMES,
    ))
    print("camera tuning: exposure=%d gain=%d contrast=%d" % (
        CAMERA_EXPOSURE,
        CAMERA_GAIN,
        CAMERA_CONTRAST,
    ))
    print("target laser mode: refresh=%d freeze=%s hold=%d" % (
        TARGET_DETECT_EVERY_N_FRAMES_WHEN_LASER,
        str(TARGET_FREEZE_WHEN_LASER),
        TARGET_FREEZE_HOLD_FRAMES_AFTER_LASER,
    ))
    if laser_calibrating():
        print(
            "laser calibration: keep laser OFF for %d base frames and %d target frames"
            % (LASER_BACKGROUND_CALIB_FRAMES, LASER_TARGET_BACKGROUND_CALIB_FRAMES)
        )
