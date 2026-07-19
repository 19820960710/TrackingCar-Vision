# 2026-07-14 添加 V2 相机与 UART0 最小链路基准

## 修改目标

- 在不识别靶标、不识别激光、不控制电机的前提下，为 V2 提供相机预览、三种 60 FPS 候选分辨率基准、初始相机锁定参数和周期 UART0 安全测试帧。

## 修改内容

1. 扩展相机驱动
   - 支持按指定宽、高、FPS 创建相机。
   - 增加曝光、增益和四通道手动白平衡的初始锁定，以及预热帧。
2. 增加实机工具
   - `tools/camera_preview.py` 只显示相机画面。
   - `tools/hardware_baseline.py` 每次测试一个候选分辨率，抓取 300 帧、统计实际 FPS 与帧间隔 P50/P95，并每 6 帧发送一次安全 LOST 帧。
3. 固化候选与记录表
   - 固定 320×240、512×320、448×448 三个 60 FPS 候选。
   - 新增中文实机基准文档和待填写结果表。
4. 增加无硬件计时测试
   - 覆盖百分位计算、滑动时间窗口容量、候选选择和 UART 测试帧安全性。

## 涉及文件

- `config.py`
- `drivers/camera_device.py`
- `tools/benchmark.py`
- `tools/camera_preview.py`
- `tools/hardware_baseline.py`
- `docs/hardware_baseline.md`
- `tests/test_benchmark.py`
- `tests/test_hardware_baseline.py`
- `app.yaml`
- `.log/change/2026-07-14_add-v2-hardware-baseline.md`

## 验证情况

- 已对工程内 53 个 Python 文件执行内存语法编译，通过。
- 已执行 `python -B -m unittest discover -s tests -v`：18 项单元测试全部通过。
- 未在 MaixCAM Pro 和 MSPM0 实机执行；FPS、端到端延迟、循环重启和接收稳定性均待填写实机记录表。

## 未处理事项

- 初始曝光 2800、增益 1 和白平衡数组仅是起点，必须按最终场地实测后回填。
- 蓝紫激光、靶框、单应性和电机控制不在本步骤实现。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `C:\Users\Lenovo\maixpy-official-reference\docs\doc\zh\vision\camera.md`
- Source: `C:\Users\Lenovo\TrackingCar-Vision\maixcam\drivers\camera_device.py`（只读参考）
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\docs\interface_contract.md`
- Tool: `functions.apply_patch` 生成本步骤代码、文档、测试和日志。
