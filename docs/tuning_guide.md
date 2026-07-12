# Tuning Guide

Use this file when the target box, laser point, or FPS is not good enough.

## First Rule

Change one thing at a time, then write the result in `docs/experiment_log.md`.

## Target Center Is Wrong

Check the red outline first.

If the red quadrilateral follows the target but the center is still off:

```python
TARGET_SMOOTHING_ALPHA_X100 = 45
```

Increase it for faster response. Decrease it for less jitter.

If the target jumps when switching between perspective detection and blob fallback, tune:

```python
TARGET_SMOOTH_MAX_JUMP = 80
```

A larger value smooths more mode changes, but too large a value can hide a real wrong detection.

If the red outline is missing or jumps:

```python
TARGET_RECT_THRESHOLD = 10000
TARGET_PERSPECTIVE_MIN_AREA = 1200
TARGET_PERSPECTIVE_MAX_ASPECT_X100 = 450
```

Use `TARGET_MODE = "blob"` only as a fast fallback test.

## Green Laser Is Not Detected

Start the program with the laser off. Wait until both `laser: CAL BASE ... KEEP OFF` and `laser: CAL TARGET ... KEEP OFF` finish, then turn the laser on. If the laser was on during calibration, restart the program so the real laser is not learned as a static reflection.

If the real laser still shows `LOST`, relax these slightly:

```python
LASER_GREEN_THRESHOLDS = [
    [68, 100, -128, -10, -128, 127],
    [50, 100, -128, -2, -128, 127],
    [35, 100, -128, -12, -128, 127],
]
LASER_MIN_DENSITY_X100 = 15
LASER_MIN_ROUNDNESS_X100 = 5
LASER_AREA_MAX = 320
LASER_MAX_W = 42
LASER_MAX_H = 42
```

Try `L_min = 60` before making the A/B range wider.

If the laser is lost for one frame when the target briefly disappears, test this carefully:

```python
LASER_TARGET_LOST_FALLBACK_FRAMES = 1
```

Keep it at `0` for the safest competition default. Larger values increase the chance of selecting green reflections.

## Laser False Positives

If there is no laser but a point is reported:

```python
LASER_BACKGROUND_CALIB_FRAMES = 25
LASER_TARGET_BACKGROUND_CALIB_FRAMES = 15
LASER_STATIC_REJECT_DISTANCE = 18
LASER_STATIC_MIN_HITS = 3
LASER_CENTER_METHOD = "blob"
LASER_MIN_DENSITY_X100 = 12
LASER_MIN_ROUNDNESS_X100 = 0
LASER_MAX_ELONGATION_X100 = 100
LASER_SCORE_MIN = 0
LASER_AREA_MAX = 320
LASER_LOST_HOLD_FRAMES = 2
LASER_REFINE_CORE = False
LASER_REFINE_MARGIN = 6
LASER_JITTER_DISTANCE = 0
LASER_JITTER_SMOOTHING_ALPHA_X100 = 100
LASER_STICK_DISTANCE = 0
LASER_STICK_SCORE_MARGIN = 0
```

Increase calibration frames to learn more fixed reflections. `LASER_BACKGROUND_CALIB_FRAMES` covers the center ROI, and `LASER_TARGET_BACKGROUND_CALIB_FRAMES` covers the target area after the red target is found. Increase `LASER_MIN_ROUNDNESS_X100` or `LASER_SCORE_MIN` if white-background reflections are still being selected. Decrease `LASER_AREA_MAX` if large green patches are being selected.

When the green laser is on, `TARGET_FREEZE_WHEN_LASER = True` freezes the red target box briefly after recent laser detections. `TARGET_FREEZE_HOLD_FRAMES_AFTER_LASER = 2` protects the red box during confirmation/lost frames, then `TARGET_DETECT_EVERY_N_FRAMES_WHEN_LASER = 3` allows low-rate target refresh so it can still follow movement.

Use jitter smoothing only after the raw laser center is basically correct. If the raw marker is correct but jittery, test `LASER_JITTER_DISTANCE = 12` and `LASER_JITTER_SMOOTHING_ALPHA_X100 = 45`.

Use sticky candidate selection only after the raw laser center is basically correct. If the marker jumps between nearby green reflections, test `LASER_STICK_DISTANCE = 24` and `LASER_STICK_SCORE_MARGIN = 180`.

If the marker is consistently offset while the laser is still, check exposure first. If the laser center looks white, reduce `CAMERA_EXPOSURE` or `CAMERA_GAIN` before changing thresholds.

## FPS Is Too Low

Fastest safe adjustments:

```python
PRINT_FPS = False
SHOW_VERBOSE_STATUS = False
SHOW_CENTER_GUIDE = False
SHOW_GRID = False
SHOW_ROI = False
DETECT_EVERY_N_FRAMES = 2
```

If it is still too slow, try:

```python
PRINT_TIMING = True
TIMING_PRINT_EVERY_N_FRAMES = 60
```

This prints coarse target/laser/total processing time every 60 frames. Turn it back off before the formal run.

Then reduce debug drawing further:

```python
DETECT_EVERY_N_FRAMES = 3
SHOW_STATUS_TEXT = False
```

Use `SHOW_VERBOSE_STATUS = True` only when checking config, ROI, or image size. Keep it off for normal aiming tests.

Only reduce resolution after the above changes:

```python
CAMERA_WIDTH = 480
CAMERA_HEIGHT = 300
```

## Camera Brightness Drifts

If the laser center looks white on paper, start with low fixed exposure and gain:

```python
CAMERA_EXPOSURE = 2800
CAMERA_GAIN = 1
```

If the laser is still insensitive, try `CAMERA_EXPOSURE = 3000`. If the laser center turns white again, try `CAMERA_EXPOSURE = 2500`.

## UART Output

UART is off by default:

```python
ENABLE_UART_OUTPUT = False
```

When the main controller is ready, enable it and use aim mode:

```python
ENABLE_UART_OUTPUT = True
UART_OUTPUT_MODE = "aim"
```
