# Official MaixPy Reference

## References

- MaixPy camera docs: https://wiki.sipeed.com/maixpy/doc/en/vision/camera.html
- MaixPy find blobs docs: https://wiki.sipeed.com/maixpy/doc/en/vision/find_blobs.html
- Official 2025 E demo: https://github.com/sipeed/MaixPy/tree/main/projects/demo_diansai_2025_E_circle_track

## What We Have Adopted

The current `maixcam/main.py` stays as a lightweight real-time baseline:

- Use `find_blobs` with LAB thresholds to detect dark target candidates.
- Use ROI, area threshold, and pixel threshold to reduce false candidates.
- Use `blob.cx()` and `blob.cy()` as the center when available.
- Hide the red blob box by default because the box can deform heavily when the camera is tilted.
- Use `buff_num=1` to reduce camera capture latency.
- Skip the first few camera frames after startup.
- Keep camera contrast configurable.

## Official 2025 E Demo Takeaways

The official E-question demo is stronger for angled views, but it is also heavier:

- It can use a YOLOv5 model to find the black A4 frame first.
- It crops the target area and finds the inner rectangle corners.
- It uses perspective transform to convert the tilted A4 paper into a standard front-view image.
- It computes the center in the standard image, then maps the center point back to the original camera image.
- It can optionally find circles again inside the transformed standard image.

This is the right long-term direction for solving the tilted-camera problem. It should be added as a separate robust mode after the current baseline is stable, because the official version needs extra model files and OpenCV/Numpy support on the board.

## Upgrade Plan

1. Keep the current blob baseline as the fast runnable version.
2. Collect test images from the actual camera: front view, left tilt, right tilt, high/low angle, strong light, weak light.
3. Tune camera contrast, exposure, gain, and LAB thresholds using those samples.
4. Add a separate `perspective` target mode:
   - detect the black A4 frame,
   - find four valid corners,
   - compute a standard A4 view,
   - map the center back to the original image.
5. Add UART output only after the center point is stable enough.

## Notes For Testing

If the screen becomes too dark or edges become too harsh, lower:

```python
CAMERA_CONTRAST = 50
```

If the target center jumps, first keep:

```python
SHOW_TARGET_BOX = False
```

Then tune:

```python
TARGET_BLOB_THRESHOLDS
TARGET_BLOB_AREA_MIN
TARGET_BLOB_PIXELS_MIN
TARGET_SMOOTHING_ALPHA_X100
```
