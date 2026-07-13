# 2026-07-13 blob 引导矩形角点识别

## 修改目标

- 按“先用 `find_blobs` 找视野里最大的黑色框，再在该框范围内调用 `find_rects` 找四个角点”的思路调整矩形识别逻辑。
- 减少侧视、远距离和背景边缘对四角识别的干扰，保持输出靶心坐标接口不变，方便后续云台联调。

## 修改内容

1. 新增 blob 引导矩形识别参数。
   - `TARGET_RECT_USE_BLOB_ROI = True`
   - `TARGET_RECT_BLOB_ROI_MARGIN = 24`
   - `TARGET_RECT_BLOB_ROI_FALLBACK = False`
2. 修改 `safe_find_rects()` 和 `safe_find_blobs()`。
   - 支持外部传入 ROI。
   - 不传 ROI 时仍使用原来的 `target_search_roi()`。
3. 新增 `find_largest_black_blob_roi()`。
   - 在当前目标搜索区域中用 `find_blobs` 找黑色目标。
   - 选择像素数最大的候选黑色框。
   - 将该框外扩后作为 `find_rects` 的精定位 ROI。
4. 重构 `detect_perspective_rects()`。
   - 先调用 `find_largest_black_blob_roi()` 获取黑框 ROI。
   - 再在该 ROI 内调用 `find_rects` 找四角。
   - 由 `perspective_target_from_rects()` 统一筛选四角、计算对角线交点和评分。
5. 保持目标输出格式不变。
   - 仍输出 `type = "perspective"`、`x/y`、`rect`、`corners`、`score`。
   - 后续云台接口仍可继续使用目标中心坐标和 `aim dx/dy`。

## 涉及文件

- `maixcam/config.py`
- `maixcam/settings.py`
- `maixcam/vision/target.py`
- `.log/change/2026-07-13_blob-guided-rect-target.md`

## 验证情况

- 已运行语法检查：
  `python -m py_compile maixcam\main.py maixcam\settings.py maixcam\app\pipeline.py maixcam\app\overlay.py maixcam\app\runtime_state.py maixcam\drivers\camera_device.py maixcam\drivers\uart_output.py maixcam\vision\geometry.py maixcam\vision\target.py maixcam\vision\laser.py`
- 已检查 `TARGET_RECT_BLOB_ROI_*` 参数在 `config.py` 和 `settings.py` 中存在，并在 `vision/target.py` 中使用。
- 未在 MaixCam 真机侧视场景验证识别稳定性，需要上板观察红框和靶心坐标。

## 未处理事项

- 未更改串口输出协议。
- 未修改云台控制代码。
- 若侧视时最大黑色 blob 包含非目标黑色物体，需要继续增加面积、位置或连续性约束。

## 依据与工具

- Skill: `D:\Documents\Desktop\2026电赛\project-logbook-skill-2026-07-09\skills\project-logbook\SKILL.md`
- Source: `D:\Documents\Desktop\2026电赛\project-logbook-skill-2026-07-09\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\电赛\TrackingCar-Vision\maixcam\config.py`
- Source: `D:\Documents\电赛\TrackingCar-Vision\maixcam\settings.py`
- Source: `D:\Documents\电赛\TrackingCar-Vision\maixcam\vision\target.py`
- Tool: `functions.shell_command`, command `python -m py_compile maixcam\main.py maixcam\settings.py maixcam\app\pipeline.py maixcam\app\overlay.py maixcam\app\runtime_state.py maixcam\drivers\camera_device.py maixcam\drivers\uart_output.py maixcam\vision\geometry.py maixcam\vision\target.py maixcam\vision\laser.py`, cwd `D:\Documents\电赛\TrackingCar-Vision`
- Tool: `functions.apply_patch`, cwd `D:\Documents\电赛\TrackingCar-Vision`
