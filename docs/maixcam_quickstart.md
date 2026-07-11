# MaixCam Pro Quickstart

## Run Camera Preview

1. Connect MaixCam Pro to the computer.
2. Open MaixVision.
3. Open `maixcam/camera_preview.py`.
4. Click Run.
5. Check that the camera image appears on the device screen.

## Recommended First Settings

Use this first:

```python
CAMERA_WIDTH = 320
CAMERA_HEIGHT = 240
```

This resolution is easier for real-time target detection and aiming. Increase it only after the basic recognition pipeline is stable.

## Next Vision Stages

After preview works:

1. Add target detection in `maixcam/main.py`.
2. Add laser spot detection.
3. Calculate `dx` and `dy`.
4. Send aiming data to the main controller by UART.
