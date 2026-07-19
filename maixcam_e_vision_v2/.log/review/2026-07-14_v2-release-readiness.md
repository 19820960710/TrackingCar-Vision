# 2026-07-14 V2 整机验收与发布前审查

## 检查范围

- `D:\Documents\Desktop\MaixCAM-E-Vision-V2` 的主入口、流水线、目标重捕、激光、毫米标定、UART 协议、性能工具和现有测试。
- 不检查旧仓库，不修改 MSPM0 固件或张大头电机控制工程。

## 检查依据

- 用户定义的整机验收项目：启动日志、遮挡/强光/反光/快速移动/丢失、串口断线/旧数据/预测超时、张大头方向/零点/软限位/失控保护、长时间运行。
- V2 接口契约、二进制协议安全文档、性能记录表和 V2 当前配置。

## 发现

1. 发布阻塞：主入口从未执行“激光关闭背景标定”。
   - 影响：`main.py` 的每帧上下文固定为 `laser_calibration=False`，`LaserDetector.background` 始终没有稳定背景记录；反光/背景亮点排除的验收前提不成立。
   - 证据：`main.py` 主循环；`vision/laser/detector.py` 仅在 `context["laser_calibration"]` 为真时调用 `background.observe()`。
   - 处理：需要修复。应在发布前提供明确的启动标定阶段或受控开关，累计至少 25 帧并显示 `BACKGROUND_READY` 后才进入正常激光识别。
2. 发布阻塞：毫米映射与画圆未获得实物标定。
   - 影响：`PLANE_WHITE_*` 和 `PLANE_OUTER_*` 均为 `None`，主入口会禁用标准平面，无法验收毫米距离和 60 mm 圆误差。
   - 证据：`config.py`、`main.py:optional_plane_mapper()`、`docs/plane_measurement.md`。
   - 处理：需要实机测量。先填入成品靶纸实测白框/外框尺寸，再用尺记录两点距离和圆轨迹误差；不得调整滤波掩盖比例错误。
3. 发布阻塞：YOLO 全局重捕尚未接入模型资产。
   - 影响：默认 `AiRecapture()` 的 `model=None`，实际运行只能走低分辨率黑框回退，不能宣称完成 YOLO 全局重捕验收。
   - 证据：`vision/target/ai_recapture.py`；`app/state_machine.py` 默认构造 `AiRecapture()`。
   - 处理：需要实机验证。接入满足 `{valid, rect, confidence}` 契约的模型，记录 AI 重捕 P95 和复杂背景成功率。
4. 发布阻塞：MSPM0 和张大头机械安全没有在当前工程中实现或验收。
   - 影响：方向、零点、软限位、失控保护、启动日志过滤、100 ms 看门狗、旧帧/CRC 策略均不能由视觉端代码代替。
   - 证据：`docs/binary_protocol.md` 明确安全参考不是 MCU 固件；V2 中不存在 MSPM0 电机工程。
   - 处理：需要主控实机验收。MSPM0 端必须记录并通过每项安全动作；不通过时禁止电机闭环跟随。
5. 发布阻塞：性能与长时间运行指标没有实测记录。
   - 影响：虽然计时窗口有固定容量 120，未发现单帧性能统计的无界积累，但尚未证明 12 ms P95、端到端延迟、AI 重捕耗时、UART 收帧时间、长稳无重启或错误锁定。
   - 证据：`config.py` 的 12 ms 预算、`tools/pipeline_benchmark.py`、`docs/pipeline_benchmark.md` 均为待测表；无硬件结果文件。
   - 处理：需要实机验证。连续运行并填写稳定跟踪、AI 重捕、黑框回退三类 P95；由 MSPM0 记录 T2 以计算 UART/端到端延迟。
6. 安全设计已存在但仍需硬件确认：协议和异常隔离。
   - 影响：代码级测试表明目标/激光异常会降级为无效量测；二进制参考能区分正常、预测、丢失、旧帧和 CRC 错帧。但 ASCII 当前没有序号/CRC，且 MSPM0 尚未移植二进制参考。
   - 证据：`app/pipeline.py`、`protocol/binary_v1.py`、`protocol/mspm0_safety_reference.py`、`tests/test_binary_protocol.py`。
   - 处理：需要实机验证。保持 `UART_PROTOCOL_MODE="ascii_aim"`；仅在 MSPM0 固件移植并验收后启用二进制。

## 已采纳

- 无代码修改。本次仅形成发布前问题清单和验收门槛。

## 未采纳

- 不将桌面 48 项单元测试或 Python 协议参考视为 MaixCAM Pro、MSPM0 或电机整机验收通过；原因是当前没有真实硬件结果。

## 验证情况

- 已审阅主入口、流水线、目标重捕、激光、标定、协议和性能文档。
- 已参考此前全工程结果：68 个 Python 文件内存语法编译通过，48 项单元测试通过。
- 未执行相机、UART、MSPM0、张大头电机或长时间实机测试，原因是当前桌面环境未连接这些硬件。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-review-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\main.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\pipeline.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\laser\detector.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\target\ai_recapture.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\docs\binary_protocol.md`
- Tool: `functions.exec` read-only inspection, cwd `C:\Users\Lenovo`.
