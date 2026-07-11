# Official MaixPy Reference Notes

## References

- MaixPy API reference: https://wiki.sipeed.com/maixpy/api/
- MaixPy camera docs: https://wiki.sipeed.com/maixpy/doc/en/vision/camera.html
- MaixPy find blobs docs: https://wiki.sipeed.com/maixpy/doc/en/vision/find_blobs.html
- Official 2025 E demo: https://github.com/sipeed/MaixPy/tree/main/projects/demo_diansai_2025_E_circle_track

## What This Project Uses Now

The current `maixcam/main.py` stays lightweight and runnable:

- `camera.Camera(width, height, fps=..., buff_num=1)` when supported
- `img.find_rects(...)` for tilted rectangle/corner detection
- diagonal intersection of four corners as target center
- blob fallback when the perspective rectangle is missing
- `img.find_blobs(...)` for green laser candidates
- target-area laser search instead of full-frame search
- startup background calibration to reject static green reflections
- optional UART output for `AIM` data

## Notes From Your Reference Folder

Useful ideas adopted:

- Request camera FPS explicitly, as shown in several MaixCam examples.
- Avoid printing FPS every frame on the board.
- Use small ROI and target-guided search for speed.
- Use startup background/static-point filtering inspired by the dynamic light-point tracker.
- Keep heavy OpenCV rectangle and background pipelines as future robust modes, not the default live path.

## Official 2025 E Demo Takeaways

The official E-question demo is stronger for angled views, but heavier:

- It can find the black A4 frame first.
- It crops the target area.
- It can perform perspective transform into a standard front-view image.
- It computes the target center in the transformed image.
- It maps the center back to the original camera image.

This is the long-term robust direction. Keep it as a separate mode after the current MaixPy-native baseline is stable.

## Upgrade Plan

1. Stabilize current `perspective` mode on real target board.
2. Record test images for front view, tilted view, strong light, weak light, and moving target.
3. Tune camera exposure/gain and LAB thresholds from those samples.
4. Add a separate `robust_perspective` mode using OpenCV only if the native mode is not enough.
5. Enable UART `AIM` output after target and laser are both stable.

## Current Testing Defaults

```python
TARGET_MODE = "perspective"
TARGET_PERSPECTIVE_FALLBACK_BLOB = True
LASER_COLOR = "green"
LASER_USE_BACKGROUND_CALIB = True
UART_OUTPUT_MODE = "aim"
```
