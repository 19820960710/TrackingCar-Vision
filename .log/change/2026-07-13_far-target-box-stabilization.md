# 2026-07-13 远距离目标红框尺寸稳定

## 修改目标

- 解决摄像头距离靶子约 1-1.5 米时，红色识别框大小反复缩放、不能稳定贴住黑色矩形框的问题。
- 保持当前目标检测、激光检测、坐标显示、UART 输出和云台联调接口能力不关闭。

## 修改内容

1. 新增目标角点平滑参数。
   - `TARGET_CORNER_SMOOTHING_ALPHA_X100 = 45`
   - 用于抑制远距离小目标下 `find_rects` 四角点在黑框内外边缘间跳动。
2. 新增目标尺寸平滑参数。
   - `TARGET_SIZE_SMOOTHING_ALPHA_X100 = 45`
   - 用于无四角或 fallback 目标时平滑外接矩形宽高。
3. 修改 `vision/target.py` 的 `smooth_target_rect()`。
   - 当当前目标和上一帧目标都有 `corners` 时，逐点平滑四个角点。
   - 平滑角点后重新计算 `rect` 和目标中心，保持红框、中心点和后续云台坐标一致。
   - 当目标只有 `rect` 时，平滑矩形宽高，再按平滑中心回推矩形左上角。
4. 同步 `settings.py` fallback 参数。
   - 确保 `config.py` 缺失时仍有相同的角点/尺寸平滑策略。

## 涉及文件

- `maixcam/config.py`
- `maixcam/settings.py`
- `maixcam/vision/target.py`
- `.log/change/2026-07-13_far-target-box-stabilization.md`

## 验证情况

- 已运行语法检查：
  `python -m py_compile maixcam\main.py maixcam\settings.py maixcam\app\pipeline.py maixcam\app\overlay.py maixcam\app\runtime_state.py maixcam\drivers\camera_device.py maixcam\drivers\uart_output.py maixcam\vision\geometry.py maixcam\vision\target.py maixcam\vision\laser.py`
- 已检查新增参数在 `config.py` 和 `settings.py` 中存在，并在 `vision/target.py` 中被使用。
- 未在 MaixCam 真机上验证 1-1.5 米距离下红框缩放抖动是否消失，需要上板实测。

## 未处理事项

- 未修改摄像头分辨率和目标检测阈值，避免引入新的识别范围变化。
- 未关闭坐标显示、激光识别或 UART 输出。
- 若实测仍有缩放，需要继续判断抖动来源是四角检测边缘跳变、blob fallback 切换，还是相机曝光/增益导致黑框边缘不稳定。

## 依据与工具

- Skill: `D:\Documents\Desktop\2026电赛\project-logbook-skill-2026-07-09\skills\project-logbook\SKILL.md`
- Source: `D:\Documents\Desktop\2026电赛\project-logbook-skill-2026-07-09\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\电赛\TrackingCar-Vision\maixcam\config.py`
- Source: `D:\Documents\电赛\TrackingCar-Vision\maixcam\settings.py`
- Source: `D:\Documents\电赛\TrackingCar-Vision\maixcam\vision\target.py`
- Tool: `functions.shell_command`, command `python -m py_compile maixcam\main.py maixcam\settings.py maixcam\app\pipeline.py maixcam\app\overlay.py maixcam\app\runtime_state.py maixcam\drivers\camera_device.py maixcam\drivers\uart_output.py maixcam\vision\geometry.py maixcam\vision\target.py maixcam\vision\laser.py`, cwd `D:\Documents\电赛\TrackingCar-Vision`
- Tool: `functions.apply_patch`, cwd `D:\Documents\电赛\TrackingCar-Vision`
