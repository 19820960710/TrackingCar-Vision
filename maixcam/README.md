# MaixCam Pro Scripts

This folder contains scripts intended to run on MaixCam Pro with MaixPy.

## Files

- `camera_preview.py`: simplest camera preview test
- `main.py`: main target + green laser + aiming pipeline
- `config.py`: runtime parameters copied by `main.py`

`main.py` also contains fallback settings, so it can run even when MaixVision uploads only this one file.

## Recommended Run Order

1. Run `camera_preview.py` to confirm the camera image is clear.
2. Run `main.py` with the green laser off.
3. Wait for `laser: CAL BASE ... KEEP OFF` to finish.
4. Keep the target visible and wait for `laser: CAL TARGET ... KEEP OFF` to finish.
5. Confirm `target: perspective (...)` or `target: blob-fallback (...)` appears.
6. Point the green laser at the target and check the blue laser marker.

## Current Pipeline

`main.py` does four jobs:

- Reads the camera with low-latency buffering and requested FPS.
- Detects the tilted target rectangle and computes the center from diagonal intersection.
- Detects the green laser only near the detected target, with startup background calibration.
- Draws debug overlay and optionally sends UART aiming data.

## Important Notes

- Keep the laser off during both base and target calibration frames.
- If `laser: CAL` never finishes, check whether `main.py` has the latest calibration fix.
- If laser detection is too strict, tune `LASER_GREEN_THRESHOLDS` and `LASER_AREA_MAX`.
- If false positives appear, increase `LASER_BACKGROUND_CALIB_FRAMES`, increase `LASER_TARGET_BACKGROUND_CALIB_FRAMES`, or lower `LASER_AREA_MAX`.
- If FPS is too low, increase `DETECT_EVERY_N_FRAMES` or hide more debug drawing.

Keep PC OpenCV code under `scripts/` and MaixPy device code under `maixcam/`.
