# MaixCam Pro Quickstart

## Run Camera Preview

1. Connect MaixCam Pro to the computer.
2. Open MaixVision.
3. Open `maixcam/camera_preview.py`.
4. Click Run.
5. Check that the camera image appears on the device screen.

## Recommended First Settings

Use this first for a clear preview:

```python
CAMERA_WIDTH = 640
CAMERA_HEIGHT = 480
```

This gives a clearer image for early debugging. If recognition becomes slow later, reduce it to `512x320` or `320x240`.

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

`maixcam/main.py` uses MaixPy built-in `find_circles` and `find_rects` first. The current default is:

```python
ENABLE_TARGET_DETECT = True
TARGET_MODE = "auto"
```

Use these modes while testing:

- `auto`: detect circles and rectangles, then choose the stronger result
- `circle`: only detect circular targets
- `rect`: only detect rectangular targets

If FPS drops too much, change `TARGET_MODE` from `auto` to the exact target shape.

## API Reference

Useful MaixPy API notes are collected in:

```text
docs/maixpy_api_notes.md
```
