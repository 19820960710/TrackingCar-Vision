try:
    from maix import app, camera, display, image, pinmap, time, uart
except ImportError:
    from maix import app, camera, display, image, time
    from maix.peripheral import pinmap, uart

try:
    from config import (
        CONFIG_SOURCE,
        CONFIG_VERSION,
        CAMERA_HEIGHT,
        CAMERA_BUFFER_NUM,
        CAMERA_CONTRAST,
        CAMERA_EXPOSURE,
        CAMERA_FPS,
        CAMERA_GAIN,
        CAMERA_SKIP_FRAMES,
        CAMERA_WB_GAIN,
        CAMERA_WIDTH,
        CROSSHAIR_SIZE,
        DETECT_EVERY_N_FRAMES,
        ENABLE_TARGET_DETECT,
        ENABLE_UART_OUTPUT,
        GRID_LINE_WIDTH,
        ENABLE_LASER_DETECT,
        LASER_AREA_MAX,
        LASER_AREA_MIN,
        LASER_CENTER_METHOD,
        LASER_COLOR,
        LASER_CONFIRM_DISTANCE,
        LASER_CONFIRM_FRAMES,
        LASER_GREEN_THRESHOLDS,
        LASER_MAX_ELONGATION_X100,
        LASER_MAX_ASPECT_X100,
        LASER_MAX_H,
        LASER_MAX_W,
        LASER_MIN_H,
        LASER_MIN_ROUNDNESS_X100,
        LASER_MIN_W,
        LASER_PIXELS_MIN,
        LASER_JITTER_DISTANCE,
        LASER_JITTER_SMOOTHING_ALPHA_X100,
        LASER_RED_THRESHOLDS,
        LASER_FALLBACK_FULL_FRAME,
        LASER_BACKGROUND_CALIB_FRAMES,
        LASER_LOST_HOLD_FRAMES,
        LASER_MIN_DENSITY_X100,
        LASER_REFINE_CORE,
        LASER_REFINE_MARGIN,
        LASER_REQUIRE_TARGET,
        LASER_SCORE_MIN,
        LASER_STATIC_MIN_HITS,
        LASER_STATIC_REJECT_DISTANCE,
        LASER_STICK_DISTANCE,
        LASER_STICK_SCORE_MARGIN,
        LASER_USE_ROI,
        LASER_SMOOTHING_ALPHA_X100,
        LASER_SNAP_DISTANCE,
        LASER_TARGET_BONUS_DISTANCE,
        LASER_TARGET_BACKGROUND_CALIB_FRAMES,
        LASER_TARGET_ROI_MARGIN,
        LASER_TARGET_LOST_FALLBACK_FRAMES,
        LASER_TRACK_BONUS_DISTANCE,
        LASER_USE_BACKGROUND_CALIB,
        LASER_USE_TARGET_ROI,
        PRINT_FPS,
        PRINT_TIMING,
        TIMING_PRINT_EVERY_N_FRAMES,
        PRINT_LASER,
        PRINT_TARGET,
        ROI_SCALE_DEN,
        ROI_SCALE_NUM,
        SHOW_CENTER_GUIDE,
        SHOW_FPS,
        SHOW_GRID,
        SHOW_ROI,
        SHOW_STATUS_TEXT,
        SHOW_VERBOSE_STATUS,
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
        TARGET_LOST_HOLD_FRAMES,
        TARGET_MAX_RADIUS,
        TARGET_MIN_RADIUS,
        TARGET_MIN_RECT_H,
        TARGET_MIN_RECT_W,
        TARGET_MODE,
        TARGET_PERSPECTIVE_FALLBACK_BLOB,
        TARGET_PERSPECTIVE_MAX_ASPECT_X100,
        TARGET_PERSPECTIVE_MIN_AREA,
        TARGET_PERSPECTIVE_MIN_H,
        TARGET_PERSPECTIVE_MIN_W,
        TARGET_RECT_THRESHOLD,
        TARGET_DETECT_EVERY_N_FRAMES_WHEN_LASER,
        TARGET_FREEZE_HOLD_FRAMES_AFTER_LASER,
        TARGET_FREEZE_WHEN_LASER,
        TARGET_SMOOTH_MAX_JUMP,
        TARGET_SMOOTHING_ALPHA_X100,
        UART_BAUDRATE,
        UART_PORT,
        UART_OUTPUT_MODE,
        UART_PRINT_ERRORS,
        UART_RX_FUNC,
        UART_RX_PIN,
        UART_SEND_EVERY_N_FRAMES,
        UART_TX_FUNC,
        UART_TX_PIN,
    )
except ImportError:
    CONFIG_VERSION = "2026-07-11-competition"
    CONFIG_SOURCE = "fallback-main"
    CAMERA_WIDTH = 512
    CAMERA_HEIGHT = 320
    CAMERA_FPS = 60
    CAMERA_BUFFER_NUM = 1
    CAMERA_SKIP_FRAMES = 5
    CAMERA_CONTRAST = -1
    CAMERA_EXPOSURE = 2800
    CAMERA_GAIN = 1
    CAMERA_WB_GAIN = []
    SHOW_FPS = True
    SHOW_VERBOSE_STATUS = False
    PRINT_FPS = False
    PRINT_TIMING = False
    TIMING_PRINT_EVERY_N_FRAMES = 60
    SHOW_CENTER_GUIDE = False
    SHOW_GRID = False
    SHOW_ROI = False
    SHOW_STATUS_TEXT = True
    SHOW_TARGET_BOX = True
    CROSSHAIR_SIZE = 24
    GRID_LINE_WIDTH = 1
    ENABLE_LASER_DETECT = True
    LASER_COLOR = "green"
    LASER_RED_THRESHOLDS = [[70, 100, 35, 127, -20, 127]]
    LASER_GREEN_THRESHOLDS = [
        [68, 100, -128, -10, -128, 127],
        [55, 100, -128, -4, -128, 127],
        [35, 100, -128, -12, -128, 127],
    ]
    LASER_CENTER_METHOD = "blob"
    LASER_USE_ROI = False
    LASER_AREA_MIN = 3
    LASER_AREA_MAX = 320
    LASER_PIXELS_MIN = 3
    LASER_MIN_W = 1
    LASER_MIN_H = 1
    LASER_MAX_W = 42
    LASER_MAX_H = 42
    LASER_MAX_ASPECT_X100 = 350
    LASER_CONFIRM_FRAMES = 1
    LASER_CONFIRM_DISTANCE = 28
    LASER_REQUIRE_TARGET = True
    LASER_FALLBACK_FULL_FRAME = False
    LASER_USE_TARGET_ROI = True
    LASER_TARGET_ROI_MARGIN = 48
    LASER_TARGET_LOST_FALLBACK_FRAMES = 0
    LASER_MIN_DENSITY_X100 = 12
    LASER_MIN_ROUNDNESS_X100 = 0
    LASER_MAX_ELONGATION_X100 = 100
    LASER_SCORE_MIN = 0
    LASER_REFINE_CORE = False
    LASER_REFINE_MARGIN = 6
    LASER_USE_BACKGROUND_CALIB = True
    LASER_BACKGROUND_CALIB_FRAMES = 25
    LASER_TARGET_BACKGROUND_CALIB_FRAMES = 15
    LASER_STATIC_REJECT_DISTANCE = 18
    LASER_STATIC_MIN_HITS = 3
    LASER_TRACK_BONUS_DISTANCE = 45
    LASER_STICK_DISTANCE = 0
    LASER_STICK_SCORE_MARGIN = 0
    LASER_TARGET_BONUS_DISTANCE = 0
    LASER_SMOOTHING_ALPHA_X100 = 100
    LASER_JITTER_DISTANCE = 0
    LASER_JITTER_SMOOTHING_ALPHA_X100 = 100
    LASER_SNAP_DISTANCE = 80
    LASER_LOST_HOLD_FRAMES = 2
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
    TARGET_PERSPECTIVE_MIN_AREA = 1200
    TARGET_PERSPECTIVE_MAX_ASPECT_X100 = 450
    TARGET_BLOB_CENTER_METHOD = "rect"
    TARGET_BLOB_THRESHOLDS = [[0, 45, -128, 127, -128, 127]]
    TARGET_BLOB_AREA_MIN = 80
    TARGET_BLOB_PIXELS_MIN = 80
    TARGET_BLOB_MIN_W = 6
    TARGET_BLOB_MIN_H = 6
    TARGET_BLOB_MAX_ASPECT_X100 = 350
    TARGET_SMOOTH_MAX_JUMP = 80
    TARGET_SMOOTHING_ALPHA_X100 = 45
    TARGET_LOST_HOLD_FRAMES = 5
    TARGET_CIRCLE_THRESHOLD = 3000
    TARGET_RECT_THRESHOLD = 10000
    TARGET_MIN_RADIUS = 8
    TARGET_MAX_RADIUS = 110
    TARGET_MIN_RECT_W = 20
    TARGET_MIN_RECT_H = 20
    DETECT_EVERY_N_FRAMES = 2
    TARGET_DETECT_EVERY_N_FRAMES_WHEN_LASER = 3
    TARGET_FREEZE_WHEN_LASER = True
    TARGET_FREEZE_HOLD_FRAMES_AFTER_LASER = 2
    PRINT_TARGET = False
    ENABLE_UART_OUTPUT = False
    UART_PORT = "/dev/ttyS1"
    UART_BAUDRATE = 115200
    UART_TX_PIN = "A19"
    UART_RX_PIN = "A18"
    UART_TX_FUNC = "UART1_TX"
    UART_RX_FUNC = "UART1_RX"
    UART_OUTPUT_MODE = "aim"
    UART_SEND_EVERY_N_FRAMES = 2
    UART_PRINT_ERRORS = True


STAGE_NAME = "TARGET_DETECT"
FRAME_INDEX = 0
LAST_TARGET = None
SMOOTHED_TARGET = None
TARGET_LOST_COUNT = 0
TARGET_FREEZE_COUNT = 0
LAST_LASER = None
SMOOTHED_LASER = None
LASER_LOST_COUNT = 0
LASER_BASE_CALIB_COUNT = 0
LASER_TARGET_CALIB_COUNT = 0
LASER_TARGET_CALIB_DONE = False
LASER_STATIC_POINTS = []
LASER_CANDIDATE = None
LASER_CONFIRM_COUNT = 0
UART_DEV = None
UART_ERROR_PRINTED = False


def frame_center():
    return CAMERA_WIDTH // 2, CAMERA_HEIGHT // 2


def create_camera():
    try:
        return camera.Camera(CAMERA_WIDTH, CAMERA_HEIGHT, fps=CAMERA_FPS, buff_num=CAMERA_BUFFER_NUM)
    except TypeError:
        pass

    try:
        return camera.Camera(CAMERA_WIDTH, CAMERA_HEIGHT, fps=CAMERA_FPS)
    except TypeError:
        pass

    try:
        return camera.Camera(CAMERA_WIDTH, CAMERA_HEIGHT, buff_num=CAMERA_BUFFER_NUM)
    except TypeError:
        print("camera fps/buff_num not supported, fallback to default camera")
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


def set_manual_exposure_mode(cam):
    try:
        return cam.exp_mode(camera.AeMode.Manual)
    except Exception:
        pass

    try:
        return cam.exp_mode(1)
    except Exception as err:
        print("camera manual exposure mode failed: %s" % err)
        return None


def apply_camera_tuning(cam):
    if camera_setting_enabled(CAMERA_EXPOSURE) or camera_setting_enabled(CAMERA_GAIN):
        set_manual_exposure_mode(cam)
    if camera_setting_enabled(CAMERA_EXPOSURE):
        actual = call_camera_methods(cam, ["exposure"], CAMERA_EXPOSURE)
        if actual:
            print("camera exposure set: %d" % CAMERA_EXPOSURE)
    if camera_setting_enabled(CAMERA_GAIN):
        actual = call_camera_methods(cam, ["gain"], CAMERA_GAIN)
        if actual:
            print("camera gain set: %d" % CAMERA_GAIN)
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


def init_uart_output():
    global UART_DEV, UART_ERROR_PRINTED

    if not ENABLE_UART_OUTPUT:
        return None

    try:
        if UART_TX_PIN and UART_TX_FUNC:
            pinmap.set_pin_function(UART_TX_PIN, UART_TX_FUNC)
        if UART_RX_PIN and UART_RX_FUNC:
            pinmap.set_pin_function(UART_RX_PIN, UART_RX_FUNC)
        UART_DEV = uart.UART(UART_PORT, UART_BAUDRATE)
        print("uart output ready: %s %d" % (UART_PORT, UART_BAUDRATE))
    except Exception as err:
        UART_DEV = None
        if UART_PRINT_ERRORS and not UART_ERROR_PRINTED:
            print("uart output init failed: %s" % err)
            UART_ERROR_PRINTED = True

    return UART_DEV


def target_output_line(target):
    center_x, center_y = frame_center()

    if not target:
        return "TV,0,0,0,0,0,LOST\n"

    dx = target["x"] - center_x
    dy = target["y"] - center_y
    return "TV,1,%d,%d,%d,%d,%s\n" % (
        dx,
        dy,
        target["x"],
        target["y"],
        target["type"],
    )


def aim_output_line(target, laser):
    if target and laser:
        dx = target["x"] - laser["x"]
        dy = target["y"] - laser["y"]
        return "AIM,1,%d,%d,%d,%d,%d,%d,%s,%s\n" % (
            dx,
            dy,
            target["x"],
            target["y"],
            laser["x"],
            laser["y"],
            target["type"],
            LASER_COLOR,
        )

    if target:
        return "AIM,0,0,0,%d,%d,0,0,%s,NO_LASER\n" % (
            target["x"],
            target["y"],
            target["type"],
        )

    if laser:
        return "AIM,0,0,0,0,0,%d,%d,NO_TARGET,%s\n" % (
            laser["x"],
            laser["y"],
            LASER_COLOR,
        )

    return "AIM,0,0,0,0,0,0,0,LOST,LOST\n"


def uart_output_line(target, laser):
    if UART_OUTPUT_MODE == "target":
        return target_output_line(target)
    return aim_output_line(target, laser)


def send_uart_result(target, laser):
    global UART_ERROR_PRINTED

    if not ENABLE_UART_OUTPUT or not UART_DEV:
        return
    if UART_SEND_EVERY_N_FRAMES > 1 and FRAME_INDEX % UART_SEND_EVERY_N_FRAMES != 0:
        return

    try:
        UART_DEV.write_str(uart_output_line(target, laser))
    except Exception as err:
        if UART_PRINT_ERRORS and not UART_ERROR_PRINTED:
            print("uart output write failed: %s" % err)
            UART_ERROR_PRINTED = True


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


def target_laser_roi():
    if not LASER_USE_TARGET_ROI or not LAST_TARGET or "rect" not in LAST_TARGET:
        return None

    rect = LAST_TARGET["rect"]
    margin = LASER_TARGET_ROI_MARGIN
    return clamp_rect(
        rect[0] - margin,
        rect[1] - margin,
        rect[2] + margin * 2,
        rect[3] + margin * 2,
    )


def should_use_laser_fallback_roi():
    if not LASER_REQUIRE_TARGET:
        return True
    if LASER_TARGET_LOST_FALLBACK_FRAMES <= 0:
        return False
    if LAST_TARGET:
        return True

    lost_extra = TARGET_LOST_COUNT - TARGET_LOST_HOLD_FRAMES
    return 0 <= lost_extra < LASER_TARGET_LOST_FALLBACK_FRAMES


def laser_search_rois():
    rois = []
    target_roi = target_laser_roi()
    if target_roi:
        rois.append(target_roi)

    if LASER_REQUIRE_TARGET and not target_roi and not should_use_laser_fallback_roi():
        return rois
    if not LASER_FALLBACK_FULL_FRAME and target_roi:
        return rois

    if LASER_USE_ROI:
        fallback_roi = get_roi()
    else:
        fallback_roi = None

    if not rois or fallback_roi != rois[0]:
        rois.append(fallback_roi)
    return rois


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


def laser_core_thresholds():
    if LASER_COLOR == "green" and LASER_GREEN_THRESHOLDS:
        return [LASER_GREEN_THRESHOLDS[0]]
    return laser_thresholds()


def safe_find_laser_blobs(img, roi=None, thresholds=None):
    if thresholds is None:
        thresholds = laser_thresholds()
    try:
        if roi:
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


def laser_base_calibrating():
    return LASER_USE_BACKGROUND_CALIB and LASER_BASE_CALIB_COUNT < LASER_BACKGROUND_CALIB_FRAMES


def laser_target_calibrating():
    if not LASER_USE_BACKGROUND_CALIB:
        return False
    if laser_base_calibrating():
        return False
    if LASER_TARGET_BACKGROUND_CALIB_FRAMES <= 0 or LASER_TARGET_CALIB_DONE:
        return False
    if not target_laser_roi():
        return False
    if LAST_TARGET and "lost_hold" in LAST_TARGET:
        return False
    return LASER_TARGET_CALIB_COUNT < LASER_TARGET_BACKGROUND_CALIB_FRAMES


def laser_calibrating():
    return laser_base_calibrating() or laser_target_calibrating()


def laser_calibration_status():
    if laser_base_calibrating():
        return "BASE", LASER_BASE_CALIB_COUNT, LASER_BACKGROUND_CALIB_FRAMES
    if laser_target_calibrating():
        return "TARGET", LASER_TARGET_CALIB_COUNT, LASER_TARGET_BACKGROUND_CALIB_FRAMES
    return None, 0, 0


def laser_calibration_rois():
    if laser_base_calibrating():
        return [get_roi()]
    if laser_target_calibrating():
        roi = target_laser_roi()
        if roi:
            return [roi]
    return None


def static_point_matches(point, x, y, distance):
    return distance_xy(point["x"], point["y"], x, y) <= distance


def remember_static_laser_candidate(candidate):
    for point in LASER_STATIC_POINTS:
        if static_point_matches(point, candidate["x"], candidate["y"], LASER_STATIC_REJECT_DISTANCE):
            hits = point["hits"] + 1
            point["x"] = (point["x"] * point["hits"] + candidate["x"]) // hits
            point["y"] = (point["y"] * point["hits"] + candidate["y"]) // hits
            point["hits"] = hits
            return

    LASER_STATIC_POINTS.append({
        "x": candidate["x"],
        "y": candidate["y"],
        "hits": 1,
    })


def update_laser_background(candidates):
    global LASER_BASE_CALIB_COUNT, LASER_TARGET_CALIB_COUNT, LASER_TARGET_CALIB_DONE

    if not laser_calibrating():
        return False

    for candidate in candidates:
        remember_static_laser_candidate(candidate)

    if laser_base_calibrating():
        LASER_BASE_CALIB_COUNT += 1
    elif laser_target_calibrating():
        LASER_TARGET_CALIB_COUNT += 1
        if LASER_TARGET_CALIB_COUNT >= LASER_TARGET_BACKGROUND_CALIB_FRAMES:
            LASER_TARGET_CALIB_DONE = True
    return True


def laser_is_static_candidate(candidate):
    if laser_calibrating():
        return True

    for point in LASER_STATIC_POINTS:
        if point["hits"] < LASER_STATIC_MIN_HITS:
            continue
        if static_point_matches(point, candidate["x"], candidate["y"], LASER_STATIC_REJECT_DISTANCE):
            return True
    return False


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


def laser_candidate_score(x, y, area, pixels, density_x100, aspect_x100, roundness_x100, elongation_x100):
    extra_aspect_x100 = max(0, aspect_x100 - 100)
    score = pixels * 12 + density_x100 * 3 + roundness_x100 * 3 - area
    score -= extra_aspect_x100 * 5 + elongation_x100 * 2

    if LAST_LASER:
        dist = distance_xy(x, y, LAST_LASER["x"], LAST_LASER["y"])
        if dist <= LASER_TRACK_BONUS_DISTANCE:
            score += (LASER_TRACK_BONUS_DISTANCE - dist) * 5

    if LAST_TARGET and LASER_TARGET_BONUS_DISTANCE > 0:
        dist = distance_xy(x, y, LAST_TARGET["x"], LAST_TARGET["y"])
        if dist <= LASER_TARGET_BONUS_DISTANCE:
            score += (LASER_TARGET_BONUS_DISTANCE - dist) * 2

    return score


def laser_blob_candidates(blobs, roi=None):
    candidates = []
    for blob in blobs:
        rect = blob_rect(blob)
        x, y = laser_blob_center(blob, rect)
        w = rect[2]
        h = rect[3]
        area = rect_area(rect)
        if roi and not point_in_rect(x, y, roi):
            continue
        if area < LASER_AREA_MIN or area > LASER_AREA_MAX:
            continue
        if w < LASER_MIN_W or h < LASER_MIN_H:
            continue
        if w > LASER_MAX_W or h > LASER_MAX_H:
            continue

        pixels = blob_pixels(blob, area)
        density_x100 = pixels * 100 // max(1, area)
        if density_x100 < LASER_MIN_DENSITY_X100:
            continue

        long_side = max(w, h)
        short_side = max(1, min(w, h))
        aspect_x100 = long_side * 100 // short_side
        if aspect_x100 > LASER_MAX_ASPECT_X100:
            continue

        roundness_x100 = blob_ratio_x100(blob, "roundness", 100)
        if roundness_x100 < LASER_MIN_ROUNDNESS_X100:
            continue

        elongation_x100 = blob_ratio_x100(blob, "elongation", 0)
        if elongation_x100 > LASER_MAX_ELONGATION_X100:
            continue

        score = laser_candidate_score(x, y, area, pixels, density_x100, aspect_x100, roundness_x100, elongation_x100)
        if score < LASER_SCORE_MIN:
            continue

        candidates.append({
            "found": True,
            "type": "laser",
            "x": x,
            "y": y,
            "rect": rect,
            "score": score,
            "pixels": pixels,
            "density_x100": density_x100,
            "roundness_x100": roundness_x100,
            "elongation_x100": elongation_x100,
        })

    return candidates


def select_laser_candidate(candidates):
    best = None
    best_score = None
    sticky = None
    sticky_score = None
    for candidate in candidates:
        if laser_is_static_candidate(candidate):
            continue

        score = candidate["score"]
        if best_score is None or score > best_score:
            best_score = score
            best = candidate

        if LAST_LASER:
            dist = distance_xy(candidate["x"], candidate["y"], LAST_LASER["x"], LAST_LASER["y"])
            if dist <= LASER_STICK_DISTANCE and (sticky_score is None or score > sticky_score):
                sticky_score = score
                sticky = candidate

    if sticky and best_score is not None and sticky_score + LASER_STICK_SCORE_MARGIN >= best_score:
        sticky["sticky"] = True
        return sticky

    return best


def refine_laser_candidate(img, candidate):
    if not LASER_REFINE_CORE or LASER_COLOR != "green" or not candidate:
        return candidate
    if "rect" not in candidate:
        return candidate

    rect = candidate["rect"]
    margin = LASER_REFINE_MARGIN
    roi = clamp_rect(
        rect[0] - margin,
        rect[1] - margin,
        rect[2] + margin * 2,
        rect[3] + margin * 2,
    )
    try:
        blobs = safe_find_laser_blobs(img, roi, laser_core_thresholds())
    except Exception:
        return candidate

    refined = select_laser_candidate(laser_blob_candidates(blobs, roi))
    if refined:
        refined["refined"] = True
        refined["raw_x"] = candidate["x"]
        refined["raw_y"] = candidate["y"]
        return refined
    return candidate


def detect_laser(img):
    if not ENABLE_LASER_DETECT:
        return None

    calibration_rois = laser_calibration_rois()
    if calibration_rois is not None:
        calibration_candidates = []
        for roi in calibration_rois:
            try:
                blobs = safe_find_laser_blobs(img, roi)
            except MemoryError as err:
                print("find_laser memory low: %s" % err)
                return None
            except Exception as err:
                print("find_laser failed: %s" % err)
                return None
            calibration_candidates += laser_blob_candidates(blobs, roi)
        update_laser_background(calibration_candidates)
        return None

    rois = laser_search_rois()
    for roi in rois:
        try:
            blobs = safe_find_laser_blobs(img, roi)
        except MemoryError as err:
            print("find_laser memory low: %s" % err)
            return None
        except Exception as err:
            print("find_laser failed: %s" % err)
            return None

        candidates = laser_blob_candidates(blobs, roi)
        best = select_laser_candidate(candidates)
        if best:
            return refine_laser_candidate(img, best)

    return None


def laser_candidate_matches(candidate, raw_laser):
    if not candidate or not raw_laser:
        return False

    return (
        abs(candidate["x"] - raw_laser["x"]) <= LASER_CONFIRM_DISTANCE
        and abs(candidate["y"] - raw_laser["y"]) <= LASER_CONFIRM_DISTANCE
    )


def clone_laser(laser):
    if not laser:
        return None

    cloned = {}
    for key in laser:
        value = laser[key]
        if key == "rect":
            cloned[key] = [value[0], value[1], value[2], value[3]]
        else:
            cloned[key] = value
    cloned["raw_x"] = laser["x"]
    cloned["raw_y"] = laser["y"]
    cloned["stable"] = False
    return cloned


def smooth_laser_rect(laser, smooth_x, smooth_y, raw_x, raw_y):
    if "rect" not in laser:
        return

    dx = smooth_x - raw_x
    dy = smooth_y - raw_y
    laser["rect"][0] += dx
    laser["rect"][1] += dy


def smooth_laser(raw_laser):
    global SMOOTHED_LASER

    laser = clone_laser(raw_laser)
    if SMOOTHED_LASER:
        old_x = SMOOTHED_LASER["x"]
        old_y = SMOOTHED_LASER["y"]
        new_x = raw_laser["x"]
        new_y = raw_laser["y"]
        move = distance_xy(old_x, old_y, new_x, new_y)

        if move <= LASER_SNAP_DISTANCE:
            alpha = LASER_SMOOTHING_ALPHA_X100
            if move <= LASER_JITTER_DISTANCE:
                alpha = LASER_JITTER_SMOOTHING_ALPHA_X100
            laser["x"] = weighted_value(old_x, new_x, alpha)
            laser["y"] = weighted_value(old_y, new_y, alpha)
            laser["stable"] = True
            smooth_laser_rect(laser, laser["x"], laser["y"], new_x, new_y)

    SMOOTHED_LASER = laser
    return SMOOTHED_LASER


def update_laser_tracking(raw_laser):
    global LASER_CANDIDATE, LASER_CONFIRM_COUNT, SMOOTHED_LASER, LASER_LOST_COUNT

    if not ENABLE_LASER_DETECT:
        LASER_CANDIDATE = None
        LASER_CONFIRM_COUNT = 0
        SMOOTHED_LASER = None
        LASER_LOST_COUNT = 0
        return None

    if not raw_laser:
        LASER_CANDIDATE = None
        LASER_CONFIRM_COUNT = 0
        if SMOOTHED_LASER and LASER_LOST_COUNT < LASER_LOST_HOLD_FRAMES:
            LASER_LOST_COUNT += 1
            SMOOTHED_LASER["lost_hold"] = LASER_LOST_COUNT
            return SMOOTHED_LASER

        SMOOTHED_LASER = None
        LASER_LOST_COUNT = LASER_LOST_HOLD_FRAMES
        return None

    if LASER_CONFIRM_FRAMES <= 1:
        LASER_CANDIDATE = raw_laser
        LASER_CONFIRM_COUNT = 1
        LASER_LOST_COUNT = 0
        return smooth_laser(raw_laser)

    if laser_candidate_matches(LASER_CANDIDATE, raw_laser):
        LASER_CONFIRM_COUNT += 1
    else:
        LASER_CANDIDATE = raw_laser
        LASER_CONFIRM_COUNT = 1

    if LASER_CONFIRM_COUNT >= LASER_CONFIRM_FRAMES:
        LASER_LOST_COUNT = 0
        return smooth_laser(raw_laser)
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


def weighted_value(old_value, new_value, alpha):
    return (old_value * (100 - alpha) + new_value * alpha) // 100


def smooth_value(old_value, new_value):
    return weighted_value(old_value, new_value, TARGET_SMOOTHING_ALPHA_X100)


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


def target_can_smooth(raw_target):
    if not SMOOTHED_TARGET or not raw_target:
        return False
    if SMOOTHED_TARGET["type"] == raw_target["type"]:
        return True
    if TARGET_SMOOTH_MAX_JUMP <= 0:
        return False

    jump = distance_xy(SMOOTHED_TARGET["x"], SMOOTHED_TARGET["y"], raw_target["x"], raw_target["y"])
    return jump <= TARGET_SMOOTH_MAX_JUMP


def smooth_target(raw_target):
    target = clone_target(raw_target)
    old_x = SMOOTHED_TARGET["x"]
    old_y = SMOOTHED_TARGET["y"]
    new_x = raw_target["x"]
    new_y = raw_target["y"]
    target["x"] = smooth_value(old_x, new_x)
    target["y"] = smooth_value(old_y, new_y)
    target["stable"] = True
    smooth_target_rect(target, target["x"], target["y"], new_x, new_y)
    return target


def update_target_tracking(raw_target):
    global SMOOTHED_TARGET, TARGET_LOST_COUNT

    if raw_target:
        if target_can_smooth(raw_target):
            SMOOTHED_TARGET = smooth_target(raw_target)
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

    if SHOW_FPS:
        img.draw_string(8, 8, "FPS: %.1f %s" % (fps, fps_state(fps)), image.COLOR_GREEN)
    if not SHOW_VERBOSE_STATUS:
        return

    center_x, center_y = frame_center()
    roi_x, roi_y, roi_w, roi_h = get_roi()
    img.draw_string(8, 28, "stage: %s" % STAGE_NAME, image.COLOR_GREEN)
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
        if TARGET_FREEZE_COUNT > 0:
            freeze_text = " FZ%d" % TARGET_FREEZE_COUNT
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
    if FRAME_INDEX % TIMING_PRINT_EVERY_N_FRAMES != 0:
        return

    print("timing target=%dms laser=%dms total=%dms" % (target_ms, laser_ms, total_ms))


def process_frame(img, fps):
    """Detect the target and draw the debug view."""
    global FRAME_INDEX, LAST_TARGET, LAST_LASER, TARGET_FREEZE_COUNT

    total_start_ms = time_ticks_ms()
    target_ms = -1
    laser_ms = -1
    FRAME_INDEX += 1

    laser_first = LAST_TARGET is not None
    if laser_first:
        laser_start_ms = time_ticks_ms()
        raw_laser = detect_laser(img)
        if raw_laser and TARGET_FREEZE_HOLD_FRAMES_AFTER_LASER > 0:
            TARGET_FREEZE_COUNT = TARGET_FREEZE_HOLD_FRAMES_AFTER_LASER
        elif TARGET_FREEZE_COUNT > 0:
            TARGET_FREEZE_COUNT -= 1
        LAST_LASER = update_laser_tracking(raw_laser)
        laser_ms = elapsed_ms(laser_start_ms)

    active_laser = LAST_LASER and "lost_hold" not in LAST_LASER and not laser_calibrating()
    recent_laser = TARGET_FREEZE_COUNT > 0 or (LAST_LASER is not None and not laser_calibrating())
    target_interval = DETECT_EVERY_N_FRAMES
    if LAST_TARGET and recent_laser and TARGET_DETECT_EVERY_N_FRAMES_WHEN_LASER > target_interval:
        target_interval = TARGET_DETECT_EVERY_N_FRAMES_WHEN_LASER
    target_interval_due = (
        target_interval <= 1
        or FRAME_INDEX == 1
        or FRAME_INDEX % target_interval == 0
    )
    target_frozen = TARGET_FREEZE_COUNT > 0 and TARGET_FREEZE_WHEN_LASER
    target_due = not LAST_TARGET or (not target_frozen and target_interval_due)

    if target_due:
        target_start_ms = time_ticks_ms()
        raw_target = detect_target(img)
        target_ms = elapsed_ms(target_start_ms)
        LAST_TARGET = update_target_tracking(raw_target)

    if not laser_first:
        laser_start_ms = time_ticks_ms()
        raw_laser = detect_laser(img)
        if raw_laser and TARGET_FREEZE_HOLD_FRAMES_AFTER_LASER > 0:
            TARGET_FREEZE_COUNT = TARGET_FREEZE_HOLD_FRAMES_AFTER_LASER
        elif TARGET_FREEZE_COUNT > 0:
            TARGET_FREEZE_COUNT -= 1
        LAST_LASER = update_laser_tracking(raw_laser)
        laser_ms = elapsed_ms(laser_start_ms)

    target = LAST_TARGET
    laser = LAST_LASER
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


def main():
    print_startup_config()
    cam = create_camera()
    apply_camera_tuning(cam)
    skip_camera_startup_frames(cam)
    init_uart_output()
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
