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
CAMERA_WIDTH = 320
CAMERA_HEIGHT = 240
```

Target detection uses MaixPy image algorithms that require fast frame buffer memory. Keep `main.py` at `320x240` first. Use `camera_preview.py` for clearer preview-only testing.

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

`maixcam/main.py` uses MaixPy built-in `find_circles` and `find_rects` first. The current safe default is:

```python
ENABLE_TARGET_DETECT = True
TARGET_MODE = "circle"
```

Use these modes while testing:

- `circle`: only detect circular targets
- `rect`: only detect rectangular targets
- `auto`: detect circles and rectangles, then choose the stronger result

Use `auto` only after `circle` or `rect` works stably. If you see fast frame buffer memory errors, lower resolution or avoid `auto`.

## API Reference

Useful MaixPy API notes are collected in:

```text
docs/maixpy_api_notes.md
```
