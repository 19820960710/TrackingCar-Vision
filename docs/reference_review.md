# Reference Review

This summarizes the useful ideas found in the local MaixCam reference folder.

## Useful Files Reviewed

- Dynamic light-point tracker: dynamic background plus brightness filtering for light-point tracking.
- `e.py`: hybrid tracker, rectangle detector, UART state switching.
- `threshold_tool.py`: HSV tuning UI and OpenCV mask workflow.
- LAB color tuning examples: touch/button-based LAB threshold adjustment.
- `25/main.py`: mode switching, UART packets, camera FPS request, reduced copy usage.
- Black rectangle corner examples: OpenCV corner ordering and contour filtering.
- Offline threshold sampling examples: touch-based threshold sampling and config saving.

## Ideas Adopted Now

- Request camera FPS explicitly.
- Avoid console prints during live detection.
- Use calibration to reject fixed false laser points.
- Split laser calibration into base ROI and target ROI phases.
- Keep laser detection constrained to the target area.
- Keep heavy OpenCV processing out of the default live loop for FPS.
- Document test steps so tuning does not become guesswork.

## Ideas Kept For Later

- Touch-screen threshold tuner inside the app.
- OpenCV adaptive threshold rectangle detector.
- Full perspective transform and map-back pipeline.
- Binary UART packet format.
- Main-controller command mode switching.

These are useful, but they should be added only after the current `main.py` is stable on the real device.
