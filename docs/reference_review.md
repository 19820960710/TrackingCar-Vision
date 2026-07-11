# Reference Review

This summarizes the useful ideas found in:

```text
D:\Documents\Desktop\2026电赛\MAIXCAMpro\MAIXCAMpro
```

## Useful Files Reviewed

- `光点-逐差法.py`: dynamic background plus brightness filtering for light-point tracking
- `e.py`: hybrid tracker, rectangle detector, UART state switching
- `threshold_tool.py`: HSV tuning UI and OpenCV mask workflow
- `试试/color_cfg.py`: LAB threshold adjustment UI
- `25/main.py`: mode switching, UART packets, camera FPS request, reduced copy usage
- `打包/打包/黑色矩形识别返回4个角点.py`: OpenCV corner ordering idea
- `打包/打包/脱机取阈值.py`: touch-based threshold sampling

## Ideas Adopted Now

- Request camera FPS explicitly.
- Avoid console prints during live detection.
- Use calibration to reject fixed false laser points.
- Keep laser detection constrained to the target area.
- Keep heavy OpenCV processing out of the default live loop for FPS.
- Document test steps so tuning does not become guesswork.

## Ideas Kept For Later

- Touch-screen threshold tuner inside the app.
- OpenCV adaptive threshold rectangle detector.
- Full perspective transform and map-back pipeline.
- Binary UART packet format.
- Main-controller command mode switching.

These are useful, but they should be added only after the current `main.py` is stable.
