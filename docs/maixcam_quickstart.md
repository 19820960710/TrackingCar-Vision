# MaixCam Pro Quickstart

## Run Camera Preview

1. Connect MaixCam Pro to the computer.
2. Open MaixVision.
3. Open `maixcam/camera_preview.py`.
4. Click Run.
5. Check that the camera image appears on the device screen.

## Recommended First Settings

Use this first for a clearer preview:

```python
CAMERA_WIDTH = 512
CAMERA_HEIGHT = 320
```

This matches Sipeed's common MaixCam camera preview example and gives better detail than `320x240`. If recognition becomes slow later, reduce it back to `320x240`.

## Next Vision Stages

After preview works:

1. Add target detection in `maixcam/main.py`.
2. Add laser spot detection.
3. Calculate `dx` and `dy`.
4. Send aiming data to the main controller by UART.
