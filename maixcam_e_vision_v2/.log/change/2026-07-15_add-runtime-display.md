# 2026-07-15 接入主运行时屏幕显示

## 修改目标

- 解决运行 `main.py` 时 MaixCAM 屏幕不显示相机画面的问题。

## 修改内容

1. 在 `drivers/display.py` 增加显示设备封装。
   - 创建 MaixPy `display.Display()`，并通过 `show()` 输出当前相机帧。
   - 保留 `ENABLE_DEBUG_DRAW` 和 `draw_debug()` 边界；当前未启用调试绘制时直接显示原始帧。
2. 在 `app/runtime.py` 为 `VisionRuntime` 增加可选显示设备。
   - 每帧完成视觉流水线处理后，将同一帧和本帧观测结果交给显示设备。
   - 未传入显示设备时保持原有桌面测试和基准工具行为。
3. 在 `main.py` 创建显示设备并注入主运行时。
4. 新增显示链路单元测试，覆盖显示封装送屏和运行时每帧送屏。

## 涉及文件

- `drivers/display.py`
- `app/runtime.py`
- `main.py`
- `tests/test_display.py`

## 验证情况

- 已运行 `python -B -m unittest discover -s tests -v`：50 项测试通过，包括新增 2 项显示测试。
- 未在 MaixCAM 实机运行 `main.py`；桌面环境没有 MaixCAM 显示硬件，因此屏幕点亮、刷新率和长时间稳定性仍需实机确认。

## 未处理事项

- `draw_debug()` 当前仍只返回原始帧，没有叠加靶框、激光点或性能信息；本次目标仅为恢复屏幕画面。
- 显示刷新耗时尚未计入性能统计，需在 MaixCAM 实机上评估送屏对帧率和 P95 延迟的影响。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\main.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\runtime.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\drivers\display.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\tools\camera_preview.py`
- Tool: `functions.exec -> shell_command`，command `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
