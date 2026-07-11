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
```

`CAMERA_CONTRAST = -1` means the program will not force camera contrast by default. Only tune it after the target center is already basically correct.

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

`maixcam/main.py` uses MaixPy built-in `find_blobs` first. The current fast default is:

```python
ENABLE_TARGET_DETECT = True
TARGET_MODE = "blob"
```

Use these modes while testing:

- `blob`: fast dark/black target candidate detection
- `circle`: only detect circular targets
- `rect`: only detect rectangular targets
- `auto`: detect blobs, circles, and rectangles, then choose the stronger result

Use `auto` only after `blob`, `circle`, or `rect` works stably. If you see fast frame buffer memory errors, lower resolution or avoid `auto`.

For blob targets, the default output is center-first. The red box is drawn around the detected center instead of using blob corner points, because blob corners can become unstable when the camera views the target at an angle.

```python
SHOW_TARGET_BOX = True
TARGET_BLOB_CENTER_METHOD = "rect"
```

The red box is a rectangular display box centered on the detected point. The target center uses the candidate rectangle center by default, not the dark-pixel center, because black rings, shadows, and uneven borders can pull the pixel center away from the real target center.

## Target Smoothing

The displayed target center is smoothed before use:

```python
TARGET_SMOOTHING_ALPHA_X100 = 35
TARGET_LOST_HOLD_FRAMES = 5
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
