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
