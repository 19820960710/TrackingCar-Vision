# 2026-07-13 目标动态 ROI 与帧率优化

## 修改目标

- 提高红框对目标矩形的贴合和响应速度。
- 在不关闭激光识别、坐标显示、UART 输出能力的前提下，降低 `find_rects` 的搜索面积，争取让实际帧率接近并稳定在 30 FPS。

## 修改内容

1. 新增目标动态 ROI 参数。
   - `TARGET_USE_TRACKING_ROI = True`
   - `TARGET_TRACKING_ROI_MARGIN = 64`
   - `TARGET_FULL_SEARCH_EVERY_N_FRAMES = 12`
2. 在 `vision/target.py` 增加 `target_search_roi()`。
   - 有上一帧目标时，只在目标矩形外扩 64 像素的局部区域内做目标搜索。
   - 目标丢失、进入 lost hold、或每 12 帧时回到全局 ROI 搜索，避免目标移出局部区域后无法找回。
3. 将目标检测频率改为每帧检测。
   - `DETECT_EVERY_N_FRAMES = 1`
   - `TARGET_DETECT_EVERY_N_FRAMES_WHEN_LASER = 1`
4. 取消激光出现后的目标冻结。
   - `TARGET_FREEZE_WHEN_LASER = False`
   - `TARGET_FREEZE_HOLD_FRAMES_AFTER_LASER = 0`
5. 调整红框响应参数。
   - `TARGET_SMOOTHING_ALPHA_X100 = 75`
   - `TARGET_LOST_HOLD_FRAMES = 3`
   - 红框显示 padding 改为 4，最小显示框改为 0，减少显示层人为放大。
6. 同步 `settings.py` fallback 参数。
   - 确保 `config.py` 缺失时，模块化版本仍使用同一套目标跟踪策略。

## 涉及文件

- `maixcam/config.py`
- `maixcam/settings.py`
- `maixcam/vision/target.py`
- `.log/change/2026-07-13_target-tracking-roi-fps.md`

## 验证情况

- 已运行语法检查：
  `python -m py_compile maixcam\main.py maixcam\settings.py maixcam\app\pipeline.py maixcam\app\overlay.py maixcam\app\runtime_state.py maixcam\drivers\camera_device.py maixcam\drivers\uart_output.py maixcam\vision\geometry.py maixcam\vision\target.py maixcam\vision\laser.py`
- 已检查关键参数在 `config.py`、`settings.py`、`vision/target.py` 中存在。
- 未在 MaixCam 真机验证实际 FPS、红框贴合效果、云台联调用的坐标稳定性。

## 未处理事项

- 未降低摄像头分辨率，当前仍是 `512x320`，避免先牺牲识别精度。
- 未关闭激光识别、底部坐标状态、UART 输出能力。
- 如果真机 FPS 仍低于 30，需要继续评估是否增加激光检测间隔、压缩状态文字绘制，或在不影响接口的前提下降低分辨率。

## 依据与工具

- Skill: `D:\Documents\Desktop\2026电赛\project-logbook-skill-2026-07-09\skills\project-logbook\SKILL.md`
- Source: `D:\Documents\Desktop\2026电赛\project-logbook-skill-2026-07-09\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\电赛\TrackingCar-Vision\maixcam\config.py`
- Source: `D:\Documents\电赛\TrackingCar-Vision\maixcam\settings.py`
- Source: `D:\Documents\电赛\TrackingCar-Vision\maixcam\vision\target.py`
- Source: `D:\Documents\电赛\TrackingCar-Vision\maixcam\app\pipeline.py`
- Tool: `functions.shell_command`, command `python -m py_compile maixcam\main.py maixcam\settings.py maixcam\app\pipeline.py maixcam\app\overlay.py maixcam\app\runtime_state.py maixcam\drivers\camera_device.py maixcam\drivers\uart_output.py maixcam\vision\geometry.py maixcam\vision\target.py maixcam\vision\laser.py`, cwd `D:\Documents\电赛\TrackingCar-Vision`
- Tool: `functions.apply_patch`, cwd `D:\Documents\电赛\TrackingCar-Vision`
