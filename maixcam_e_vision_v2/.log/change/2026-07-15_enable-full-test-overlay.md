# 2026-07-15 启用整机测试屏幕叠加

## 修改目标

- 将主程序从纯相机画面改为可直接观察目标、激光、帧率、失败原因和性能数据的整机测试界面。

## 修改内容

1. 在 `drivers/display.py` 实现目标与激光叠加。
   - 确认目标使用红色四边形或矩形，并绘制中心十字。
   - 短时预测目标使用黄色标记，避免与当前帧确认结果混淆。
   - 有效激光点使用蓝色十字和圆圈。
2. 增加屏幕诊断文字。
   - 显示平滑实时 FPS。
   - 显示目标锁定/丢失状态、置信度、坐标和恢复状态机失败原因。
   - 显示激光锁定/丢失状态和原因。
   - 显示目标检测、整帧 CPU、UART 写入的 P95 毫秒耗时。
3. 增加显示保护。
   - 叠加绘制异常时打印变化后的错误信息并继续显示原始画面，避免调试层阻断主画面。
4. 将 `config.py` 的 `ENABLE_DEBUG_DRAW` 默认改为 `True`。
5. 扩展 `tests/test_display.py`，覆盖红框、激光标记、FPS、P95、丢失原因和运行时送屏。

## 涉及文件

- `drivers/display.py`
- `config.py`
- `tests/test_display.py`

## 验证情况

- 已运行 `python -B -m unittest discover -s tests -v`：52 项测试通过，其中显示模块 4 项测试通过。
- 绘图接口依据本机旧 MaixCAM 工程中已使用的 `draw_line`、`draw_rect`、`draw_circle`、`draw_string` 和 `image.COLOR_*` 调用形式。
- 未在 MaixCAM 实机运行；屏幕字体布局、真实 FPS、叠加绘制耗时和长时间稳定性仍需实机确认。

## 未处理事项

- 未修改目标与激光检测阈值；屏幕显示的丢失原因用于后续实机判断是未找到黑框、ROI 精炼失败、确认帧不足还是激光条件不满足。
- 显示绘制耗时尚未单独加入 `PerformanceStats`；当前 FPS 能反映包括显示在内的循环间隔，但 `frame_cpu` P95 不包含 `screen.show()`。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\drivers\display.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\models.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\pipeline.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\state_machine.py`
- Source: `C:\Users\Lenovo\TrackingCar-Vision\maixcam\app\overlay.py`
- Source: `C:\Users\Lenovo\TrackingCar-Vision\maixcam\camera_preview.py`
- Tool: `functions.exec -> shell_command`，command `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
