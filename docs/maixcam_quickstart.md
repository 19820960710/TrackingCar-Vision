# MaixCam Pro Quickstart

## Run Camera Preview

1. Connect MaixCam Pro to the computer.
2. Open MaixVision.
3. Open `maixcam/camera_preview.py`.
4. Click Run.
5. Check that the camera image appears on the device screen.

## Recommended First Settings

Use this first for target detection:

```python
CAMERA_WIDTH = 512
CAMERA_HEIGHT = 320
```

Target detection uses MaixPy image algorithms that require fast frame buffer memory. `512x320` with a smaller ROI is the current balance between image clarity and speed. Use `camera_preview.py` for preview-only `640x480` testing.

The main program also uses low-latency camera buffering and skips a few startup frames:

```python
CAMERA_BUFFER_NUM = 1
CAMERA_SKIP_FRAMES = 5
CAMERA_CONTRAST = -1
PRINT_FPS = False
SHOW_GRID = False
SHOW_ROI = False
SHOW_STATUS_TEXT = False
```

`CAMERA_CONTRAST = -1` means the program will not force camera contrast by default. Only tune it after the target center is already basically correct.
`PRINT_FPS = False` keeps the IDE output window quiet. `SHOW_STATUS_TEXT = False` hides the top text block to reduce drawing overhead.

## Next Vision Stages

After preview works:

1. Run `maixcam/main.py` and confirm the center crosshair is displayed.
2. Add target detection in `maixcam/main.py`.
3. Add laser spot detection.
4. Calculate `dx` and `dy`.
5. Send aiming data to the main controller by UART.

## Code Roles

- Keep `maixcam/camera_preview.py` as the simplest camera test.
- Use `maixcam/main.py` for the real vision pipeline.
- Keep PC-only OpenCV helper scripts under `scripts/`.

`maixcam/main.py` can run as a single file in MaixVision. If `config.py` is not uploaded with it, built-in fallback settings are used.

## Debug Overlay

`maixcam/main.py` shows:

- center crosshair
- 3x3 guide grid
- center ROI rectangle
- detected target center
- target offset from image center
- FPS state
- image size and coordinate direction

The ROI is centered and covers 80% of the image by default. Change `ROI_SCALE_NUM` and `ROI_SCALE_DEN` in `maixcam/config.py` if needed.

## Target Detection

`maixcam/main.py` uses MaixPy built-in rectangle/corner detection first. Blob detection is used only to guide the search area by default. The current default is:

```python
ENABLE_TARGET_DETECT = True
TARGET_MODE = "perspective"
```

Use these modes while testing:

- `perspective`: detect the target rectangle corners and use the diagonal intersection as the target center
- `blob`: fast dark/black target candidate detection
- `circle`: only detect circular targets
- `rect`: only detect rectangular targets
- `auto`: detect blobs, circles, and rectangles, then choose the stronger result

Use `perspective` for tilted-camera target aiming. Use `auto` only after `blob`, `circle`, or `rect` works stably. If you see fast frame buffer memory errors, lower resolution or avoid `auto`.

For tilted targets, the default output is perspective-first:

```python
TARGET_MODE = "perspective"
TARGET_PERSPECTIVE_FALLBACK_BLOB = True
SHOW_TARGET_BOX = True
TARGET_BLOB_CENTER_METHOD = "rect"
```

When four corners are found, the red outline follows the tilted quadrilateral and the target center is the diagonal intersection. If corners are not found, the program briefly holds the previous target instead of drifting to a rough blob center.

For speed and stability, the perspective mode uses a small search window after the target has been found:

```python
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
```

If the target moves very fast and is lost, keep `TARGET_FULL_SCAN_INTERVAL` small or increase `TARGET_FAST_ROI_PADDING`. If the center becomes jumpy again, turn `TARGET_JUMP_REJECT_ENABLE` back on.

The rough blob target is used to guide the perspective search area, but it is not used as the final displayed target by default. This reduces drift. Only enable `TARGET_ROUGH_OUTPUT_ENABLE` if you want maximum response speed and can accept rougher center accuracy.

## Target Smoothing

The displayed target center is smoothed before use. Current tracking-priority defaults:

```python
TARGET_SMOOTHING_ALPHA_X100 = 85
TARGET_LOST_HOLD_FRAMES = 1
```

Increase `TARGET_SMOOTHING_ALPHA_X100` for faster response. Decrease it for less jitter. `TARGET_LOST_HOLD_FRAMES` keeps the last target briefly when detection drops for a few frames.

## Laser Detection

`maixcam/main.py` can detect the laser spot with `find_blobs`, but laser detection is off by default until a real laser pointer is available.

Default settings:

```python
ENABLE_LASER_DETECT = False
LASER_COLOR = "red"
```

The screen shows:

- `target`: smoothed target center
- `laser`: detected laser spot center
- `aim`: aiming error

The aiming error is:

```text
aim dx = target_x - laser_x
aim dy = target_y - laser_y
```

If you use a green laser, change:

```python
LASER_COLOR = "green"
```

If the laser is not detected, tune `LASER_RED_THRESHOLDS` or `LASER_GREEN_THRESHOLDS`.

Laser detection also requires a candidate to appear for several frames before it is reported:

```python
LASER_CONFIRM_FRAMES = 3
LASER_CONFIRM_DISTANCE = 12
```

## API Reference

Useful MaixPy API notes are collected in:

```text
docs/maixpy_api_notes.md
```

Official 2025 E-question reference notes are collected in:

```text
docs/official_maixpy_reference.md
```
