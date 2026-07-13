from settings import *
from app import runtime_state as state
from vision.geometry import *
def target_laser_roi():
    if not LASER_USE_TARGET_ROI or not state.LAST_TARGET or "rect" not in state.LAST_TARGET:
        return None

    rect = state.LAST_TARGET["rect"]
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
    if state.LAST_TARGET:
        return True

    lost_extra = state.TARGET_LOST_COUNT - TARGET_LOST_HOLD_FRAMES
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

def laser_base_calibrating():
    return LASER_USE_BACKGROUND_CALIB and state.LASER_BASE_CALIB_COUNT < LASER_BACKGROUND_CALIB_FRAMES


def laser_target_calibrating():
    if not LASER_USE_BACKGROUND_CALIB:
        return False
    if laser_base_calibrating():
        return False
    if LASER_TARGET_BACKGROUND_CALIB_FRAMES <= 0 or state.LASER_TARGET_CALIB_DONE:
        return False
    if not target_laser_roi():
        return False
    if state.LAST_TARGET and "lost_hold" in state.LAST_TARGET:
        return False
    return state.LASER_TARGET_CALIB_COUNT < LASER_TARGET_BACKGROUND_CALIB_FRAMES


def laser_calibrating():
    return laser_base_calibrating() or laser_target_calibrating()


def laser_calibration_status():
    if laser_base_calibrating():
        return "BASE", state.LASER_BASE_CALIB_COUNT, LASER_BACKGROUND_CALIB_FRAMES
    if laser_target_calibrating():
        return "TARGET", state.LASER_TARGET_CALIB_COUNT, LASER_TARGET_BACKGROUND_CALIB_FRAMES
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
    for point in state.LASER_STATIC_POINTS:
        if static_point_matches(point, candidate["x"], candidate["y"], LASER_STATIC_REJECT_DISTANCE):
            hits = point["hits"] + 1
            point["x"] = (point["x"] * point["hits"] + candidate["x"]) // hits
            point["y"] = (point["y"] * point["hits"] + candidate["y"]) // hits
            point["hits"] = hits
            return

    state.LASER_STATIC_POINTS.append({
        "x": candidate["x"],
        "y": candidate["y"],
        "hits": 1,
    })


def update_laser_background(candidates):
    if not laser_calibrating():
        return False

    for candidate in candidates:
        remember_static_laser_candidate(candidate)

    if laser_base_calibrating():
        state.LASER_BASE_CALIB_COUNT += 1
    elif laser_target_calibrating():
        state.LASER_TARGET_CALIB_COUNT += 1
        if state.LASER_TARGET_CALIB_COUNT >= LASER_TARGET_BACKGROUND_CALIB_FRAMES:
            state.LASER_TARGET_CALIB_DONE = True
    return True


def laser_is_static_candidate(candidate):
    if laser_calibrating():
        return True

    for point in state.LASER_STATIC_POINTS:
        if point["hits"] < LASER_STATIC_MIN_HITS:
            continue
        if static_point_matches(point, candidate["x"], candidate["y"], LASER_STATIC_REJECT_DISTANCE):
            return True
    return False


def laser_candidate_score(x, y, area, pixels, density_x100, aspect_x100, roundness_x100, elongation_x100):
    extra_aspect_x100 = max(0, aspect_x100 - 100)
    score = pixels * 12 + density_x100 * 3 + roundness_x100 * 3 - area
    score -= extra_aspect_x100 * 5 + elongation_x100 * 2

    if state.LAST_LASER:
        dist = distance_xy(x, y, state.LAST_LASER["x"], state.LAST_LASER["y"])
        if dist <= LASER_TRACK_BONUS_DISTANCE:
            score += (LASER_TRACK_BONUS_DISTANCE - dist) * 5

    if state.LAST_TARGET and LASER_TARGET_BONUS_DISTANCE > 0:
        dist = distance_xy(x, y, state.LAST_TARGET["x"], state.LAST_TARGET["y"])
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

        if state.LAST_LASER:
            dist = distance_xy(candidate["x"], candidate["y"], state.LAST_LASER["x"], state.LAST_LASER["y"])
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
    laser = clone_laser(raw_laser)
    if state.SMOOTHED_LASER:
        old_x = state.SMOOTHED_LASER["x"]
        old_y = state.SMOOTHED_LASER["y"]
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

    state.SMOOTHED_LASER = laser
    return state.SMOOTHED_LASER


def update_laser_tracking(raw_laser):
    if not ENABLE_LASER_DETECT:
        state.LASER_CANDIDATE = None
        state.LASER_CONFIRM_COUNT = 0
        state.SMOOTHED_LASER = None
        state.LASER_LOST_COUNT = 0
        return None

    if not raw_laser:
        state.LASER_CANDIDATE = None
        state.LASER_CONFIRM_COUNT = 0
        if state.SMOOTHED_LASER and state.LASER_LOST_COUNT < LASER_LOST_HOLD_FRAMES:
            state.LASER_LOST_COUNT += 1
            state.SMOOTHED_LASER["lost_hold"] = state.LASER_LOST_COUNT
            return state.SMOOTHED_LASER

        state.SMOOTHED_LASER = None
        state.LASER_LOST_COUNT = LASER_LOST_HOLD_FRAMES
        return None

    if LASER_CONFIRM_FRAMES <= 1:
        state.LASER_CANDIDATE = raw_laser
        state.LASER_CONFIRM_COUNT = 1
        state.LASER_LOST_COUNT = 0
        return smooth_laser(raw_laser)

    if laser_candidate_matches(state.LASER_CANDIDATE, raw_laser):
        state.LASER_CONFIRM_COUNT += 1
    else:
        state.LASER_CANDIDATE = raw_laser
        state.LASER_CONFIRM_COUNT = 1

    if state.LASER_CONFIRM_COUNT >= LASER_CONFIRM_FRAMES:
        state.LASER_LOST_COUNT = 0
        return smooth_laser(raw_laser)
    return None
