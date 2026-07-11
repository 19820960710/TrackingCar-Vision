# MaixCam Pro

This folder contains scripts intended to run on MaixCam Pro with MaixPy.

## First Camera Preview

Open this file in MaixVision and run it on the device:

```text
maixcam/camera_preview.py
```

`camera_preview.py` is standalone, so it can run even if you only open this one file in MaixVision.

Expected result:

- the MaixCam screen shows the camera image
- FPS is printed in the MaixVision console
- FPS text is drawn on the image
- press the device function key or stop the script in MaixVision to exit

## File Roles

- `camera_preview.py`: first-stage camera preview
- `main.py`: project entry with center crosshair and coordinate debug overlay
- `config.py`: device-side resolution and debug settings

`main.py` also has built-in fallback settings, so it can run even if MaixVision only uploads this one file.

Keep PC OpenCV code under `scripts/` and MaixPy code under `maixcam/`.
