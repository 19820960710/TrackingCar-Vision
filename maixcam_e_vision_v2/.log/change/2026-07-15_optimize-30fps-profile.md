# 2026-07-15 优化 30 FPS 实时配置

## 修改目标

- 在保留目标红框、激光标记和屏幕诊断信息的前提下，降低 MaixCAM 每帧计算负载，使主程序具备接近 30 FPS 的配置基础。

## 修改内容

1. 将主相机配置从 512×320 @ 60 FPS 调整为 320×240 @ 30 FPS。
   - 单帧像素数从 163840 降为 76800，减少约 53.1% 的图像输入量。
   - 保持 `CAMERA_BUFFER_NUM=1`，不增加取图缓存延迟。
2. 降低性能统计开销。
   - 增加 `PERFORMANCE_SNAPSHOT_INTERVAL=10`，P95 汇总由每帧排序改为每 10 帧更新一次。
   - `VisionRuntime` 优先复用流水线已生成的性能快照，不再正常路径中重复调用 `snapshot()`。
   - 目标、激光、UART 和屏幕输出仍每帧运行；仅诊断用 P95 数字最多延迟 10 帧更新。
3. 增加快照缓存回归测试。
   - 连续运行 11 帧时验证仅生成 2 次性能快照，并验证运行时不重复生成。

## 涉及文件

- `config.py`
- `app/pipeline.py`
- `app/runtime.py`
- `tests/test_pipeline.py`

## 验证情况

- 已运行 `python -B -m unittest discover -s tests -v`：53 项测试通过。
- 新增快照缓存测试通过：11 帧仅调用 2 次 `PerformanceStats.snapshot()`。
- 尝试运行 `python -B -m tools.pipeline_benchmark`，桌面环境因不存在 MaixPy `maix` 模块而停止，未得到实机 FPS 或 P95 数值。
- 未在 MaixCAM 实机验证 30 FPS；最终帧率取决于真实画面轮廓数量、ROI 尺寸、显示刷新、相机驱动和 UART 行为。

## 未处理事项

- 未降低目标或激光检测频率，避免控制输出在帧间使用旧检测结果。
- 未修改识别阈值。320×240 会降低远距离靶框边缘和小激光点的像素数量，需实机同时检查 FPS、红框稳定性和激光识别率；若精度不足，应在 320×240 与 512×320 间实测权衡。
- `frame_cpu` P95 仍不包含 `screen.show()`；屏幕 FPS 是判断完整循环速度的直接指标。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\config.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\pipeline.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\runtime.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\tools\pipeline_benchmark.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\docs\pipeline_benchmark.md`
- Tool: `functions.exec -> shell_command`，command `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
- Tool: `functions.exec -> shell_command`，command `python -B -m tools.pipeline_benchmark`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
