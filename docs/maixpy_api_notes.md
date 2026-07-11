# MaixPy API Notes

Official API reference:

```text
https://wiki.sipeed.com/maixpy/api/
```

Project reference notes:

```text
docs/official_maixpy_reference.md
```

## Useful Modules

- `maix.camera`: camera open, frame read, exposure, gain, mirror, flip
- `maix.display`: show images on the MaixCam screen
- `maix.image`: drawing, ROI, circle/rectangle/blob detection
- `maix.peripheral.uart`: UART communication with the main controller
- `maix.app`: program exit state
- `maix.time`: FPS and timing helpers

## Current Vision Functions

Target detection:

```python
img.find_circles(roi=[x, y, w, h], threshold=3000)
img.find_rects(roi=[x, y, w, h], threshold=10000)
img.find_blobs(thresholds, roi=[x, y, w, h], area_threshold=1000)
```

Drawing debug output:

```python
img.draw_line(x1, y1, x2, y2, image.COLOR_RED, 2)
img.draw_rect(x, y, w, h, image.COLOR_YELLOW, 2)
img.draw_circle(x, y, r, image.COLOR_GREEN, 2)
img.draw_string(x, y, "text", image.COLOR_GREEN)
```

Camera tuning:

```python
cam = camera.Camera(width, height, buff_num=1)
cam.skip_frames(5)
cam.exposure(value)
cam.gain(value)
cam.constrast(value)
cam.hmirror(value)
cam.vflip(value)
```

UART output:

```python
uart.write_str("AIM,dx,dy,state\n")
```

## Development Order

1. Make target detection stable with `find_blobs` first.
2. Add laser spot detection with `find_blobs` and compute `target - laser`.
3. Use fixed exposure and gain if color detection drifts.
4. Send `dx`, `dy`, and state to the main controller through UART.
