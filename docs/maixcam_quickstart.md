# MaixCam Pro Quickstart

## Run Camera Preview

1. Connect MaixCam Pro to the computer.
2. Open MaixVision.
3. Open `maixcam/camera_preview.py`.
4. Click Run.
5. Check that the camera image appears on the device screen.
6. Check that FPS text is drawn on the image.

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
```

`CAMERA_CONTRAST = -1` means the program will not force camera contrast by default. Only tune it after the target center is already basically correct.

## Next Vision Stages

After preview works:

1. Run `maixcam/main.py` with the green laser off.
2. Wait for `laser: CAL x/25` to finish.
3. Confirm the red target outline is reasonable.
4. Turn on the green laser and check the blue laser marker.
5. Check that the `cfg:` line shows the expected config source/version.
6. Check `aim dx/dy`.
7. Enable UART only after the screen result is stable.

## Code Roles

- Keep `maixcam/camera_preview.py` as the simplest camera test.
- Use `maixcam/main.py` for the real vision pipeline.
- Keep PC-only OpenCV helper scripts under `scripts/`.

`maixcam/main.py` can run as a single file in MaixVision. If `config.py` is not uploaded with it, built-in fallback settings are used. The debug overlay prints `cfg: external-config ...` when `config.py` is loaded and `cfg: fallback-main ...` when only `main.py` is running.

## Debug Overlay

`maixcam/main.py` shows:

- center crosshair
- optional 3x3 guide grid
- optional center ROI rectangle
- detected target center
- detected laser center
- target-to-laser aiming error
- FPS state
- image size and coordinate direction

The ROI is centered and covers 80% of the image by default. It is hidden by default for FPS, but can be shown with `SHOW_ROI = True`.

## Target Detection

`maixcam/main.py` uses MaixPy built-in rectangle/corner detection first, then falls back to blobs if the corners are not found. The current default is:

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

When four corners are found, the red outline follows the tilted quadrilateral and the target center is the diagonal intersection. If corners are not found, the program falls back to blob detection so the preview still runs.

## Target Smoothing

The displayed target center is smoothed before use:

```python
TARGET_SMOOTHING_ALPHA_X100 = 35
TARGET_LOST_HOLD_FRAMES = 5
```

Increase `TARGET_SMOOTHING_ALPHA_X100` for faster response. Decrease it for less jitter. `TARGET_LOST_HOLD_FRAMES` keeps the last target briefly when detection drops for a few frames.

## Laser Detection

`maixcam/main.py` detects the laser spot with `find_blobs`. The current default is a green laser pointer.

Default settings:

```python
ENABLE_LASER_DETECT = True
LASER_COLOR = "green"
LASER_GREEN_THRESHOLDS = [[70, 100, -128, -12, -128, 127]]
LASER_USE_ROI = False
LASER_REQUIRE_TARGET = True
LASER_FALLBACK_FULL_FRAME = False
LASER_USE_TARGET_ROI = True
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

If you switch back to a red laser, change:

```python
LASER_COLOR = "red"
```

If the laser is not detected, tune `LASER_RED_THRESHOLDS` or `LASER_GREEN_THRESHOLDS`.

Laser detection also requires a candidate to appear for several frames before it is reported:

```python
CAMERA_FPS = 60
PRINT_FPS = False
SHOW_GRID = False
SHOW_ROI = False
DETECT_EVERY_N_FRAMES = 2
LASER_CONFIRM_FRAMES = 2
LASER_CONFIRM_DISTANCE = 20
LASER_SMOOTHING_ALPHA_X100 = 85
LASER_LOST_HOLD_FRAMES = 0
```

For green laser testing, start the program with the laser off. The first frames are used to learn static green reflections, and the screen shows `laser: CAL ... KEEP OFF` during this period. If the laser was on during calibration, restart the program before trusting the result.

```python
LASER_USE_BACKGROUND_CALIB = True
LASER_BACKGROUND_CALIB_FRAMES = 25
LASER_STATIC_REJECT_DISTANCE = 18
```

## UART Output

UART output is available but disabled by default:

```python
ENABLE_UART_OUTPUT = False
UART_OUTPUT_MODE = "aim"
```

When the main controller is ready, enable it in `maixcam/config.py` or in the fallback settings at the top of `maixcam/main.py`.

The default UART line is `AIM,valid,dx,dy,target_x,target_y,laser_x,laser_y,target_mode,laser_color`.

The UART protocol is documented in:

```text
docs/uart_protocol.md
```

## Tuning And Testing

Use these two files before changing code again:

```text
docs/tuning_guide.md
docs/test_checklist.md
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
