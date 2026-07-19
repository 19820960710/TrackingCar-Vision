# 2026-07-14 V2 从头到尾发布前复审

## 检查范围

- `D:\Documents\Desktop\MaixCAM-E-Vision-V2` 的 V2 视觉工程：工程边界、数据契约、相机/UART 最小链路、几何、靶框定位、全局重捕、目标时序、激光、标准平面、协议、流水线、基准脚本与测试。
- 不检查旧工程 `D:\Documents\Desktop\TrackingCar-Vision`，本次未修改该工程。
- 不执行 MaixCAM Pro、MSPM0G3507、张大头步进电机的实机操作。

## 检查依据

- 固定硬件契约：A16/UART0_TX -> MSPM0 UART0_RX，共地；A17 仅预留双向通信；`/dev/ttyS0`、115200、8N1、`maix_comm_method=none`。
- V2 首版保持 `AIM,...` ASCII 协议；目标丢失、数据超时或串口异常时主控停止跟随。
- 既定实施顺序与完成标准：先安全数据契约，再检测、滤波、激光、单应性、串口和性能验收。
- `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-14_v2-release-readiness.md` 的前次验收准备复审记录。

## 发现

1. 当前结论为“禁止作为整机发布版本”，不是“代码不可运行”。
   - 影响：静态代码和单元测试通过，只能证明桌面环境的逻辑一致性；不能证明相机帧率、串口收帧、主控停机和电机保护已在真实硬件成立。
   - 证据：本次仅运行 Python 编译与单元测试；工程内没有实机 FPS、端到端延迟、MSPM0 收帧、步进电机限位或长稳测试记录。
   - 处理：暂不发布；先完成第 8 项整机验收中的实测项目。

2. 激光背景标定尚未被默认运行入口执行。
   - 影响：反光点与静态高亮点不能建立背景排除基线，激光模块的“标定后忽略背景亮点”策略在默认运行流程中没有生效。
   - 证据：`main.py` 调用 `runtime.run_once()` 时没有传入 `laser_calibration=True`；`vision/laser/detector.py` 只有该标志为真时才调用背景观察。
   - 处理：需要实现显式的“激光关闭标定阶段”，并在实机上验证标定帧数、背景点容量和反光场景。此项未改代码。

3. 标准平面的实际毫米尺寸尚未配置，毫米坐标和画圆任务不可验收。
   - 影响：不能输出可追溯的毫米误差，也不能把 60 mm 圆轨迹视为真实靶纸上的 60 mm。
   - 证据：`config.py` 的 `PLANE_WHITE_WIDTH_MM`、`PLANE_WHITE_HEIGHT_MM`、`PLANE_OUTER_WIDTH_MM`、`PLANE_OUTER_HEIGHT_MM` 均为 `None`；`vision/geometry/standard_plane.py` 会拒绝未实测尺寸的映射。
   - 处理：先用尺子测量实际打印靶纸白框/外框，再填入配置并用已知距离复核；未实测前不启用几何输出。

4. AI 全局重捕接口存在，但默认没有加载实际模型。
   - 影响：ROI 丢失时只能使用低分辨率黑色矩形粗提议；复杂背景下不能把“YOLO 全局重捕”当作已实现能力。
   - 证据：`main.py` 创建 `AiRecapture()` 时未传模型；`vision/target/ai_recapture.py` 在 `model is None` 时明确返回模型不可用。
   - 处理：若现场背景不能保证黑框粗提议可靠，需要选择并部署实际 MaixPy 可运行模型，再补充模型耗时和失败降级实测。

5. MSPM0 侧的安全闭环和张大头电机保护没有在本工程实现或验证。
   - 影响：即便 MaixCAM 发送 LOST，仍不能证明主控会过滤启动日志、旧帧和异常帧后停止电机，也不能证明方向、零点、软限位和失控保护正确。
   - 证据：`protocol/mspm0_safety_reference.py` 是 Python 行为参考，不是 MSPM0 固件；工程没有 MCU 源码和电机实测记录。
   - 处理：需要在 MSPM0 工程落地接收状态机与看门狗，并按真实接线进行逐项验收。此项不能由本次桌面审查替代。

6. 300 帧基准的表述与 P95 采样窗口不一致。
   - 影响：`tools/pipeline_benchmark.py` 虽运行 300 帧，但性能统计默认只保留最近 120 个样本；输出 P95 不是完整 300 帧 P95，容易误读为 300 帧整体结果。
   - 证据：`tools/pipeline_benchmark.py` 默认 `--frames 300`；`app/timing.py` 的 `PerformanceStats` 默认容量为 120，流水线使用该默认值。
   - 处理：实机基准前需要统一“滚动 120 帧 P95”或“完整 300 帧 P95”的定义，并使脚本、容量和文档一致。此项未改代码。

7. 激光背景点集合在持续标定模式下没有容量上限。
   - 影响：若误把 `laser_calibration=True` 长时间保持，新的亮点会持续加入列表，违背长时间运行不积累缓存的要求。
   - 证据：`vision/laser/background.py` 的 `observe()` 对未匹配候选点追加保存，未设置最大数量或淘汰规则。
   - 处理：标定模式应是有限帧状态；后续修复时还应增加最大背景点数或去重/淘汰策略。此项未改代码。

8. 串口发送成功不等于 MSPM0 已正确接收。
   - 影响：Maix 端 UART 写调用返回成功，仅证明本端写调用未报错；不能证明 A16 接线、共地、波特率和主控解析全部正确。
   - 证据：`drivers/uart0.py` 的成功条件是本端写调用；当前 ASCII 协议没有帧序号和 CRC。二进制 V1 有 CRC 与序号，但 `UART_PROTOCOL_MODE` 仍为 `ascii_aim`，且 MSPM0 侧只提供 Python 参考。
   - 处理：V2 首版继续保持 ASCII 兼容；实机必须用 MSPM0 回显/指示或日志确认有效 AIM 帧，二进制协议只能在 MCU 固件完成后另行启用。

9. 异常处理以安全失效为主，但可诊断性不足。
   - 影响：流水线会把靶框/激光检测异常转换为无效量测并发送安全帧，避免错误控制；但激光候选提取的底层异常被转为空候选，现场可能难以区分“确实无激光”与“OpenCV/图像输入异常”。
   - 证据：`app/pipeline.py` 对目标和激光异常转为无效量测；`vision/laser/detector.py` 的候选提取广泛捕获异常后返回空列表。
   - 处理：比赛模式保持不打印调试信息；后续增加有限频率的诊断计数或状态码，不将异常静默伪装成普通无目标。此项未改代码。

10. 现有测试覆盖模块契约和安全分支，但不覆盖真实光学条件与硬件时序。
    - 影响：单元测试通过不表示白框在轻微斜视、强光、反光、不同距离、目标快速移动时稳定，也不表示 60 FPS 或 CPU P95 < 12 ms。
    - 证据：测试位于 `tests`，使用固定输入和模拟对象；本次运行 48 项测试均通过，未连接 MaixCAM Pro 和 MSPM0G3507。
    - 处理：按下列实机清单补测：三种候选分辨率 FPS/延迟、静态与斜视靶纸、强光/反光、短遮挡、目标丢失、串口断线/旧数据、连续 30 分钟运行、步进方向/零点/软限位。

## 已采纳

- 采纳“发布前必须审查整个链路”的要求，完成工程内静态编译、全量单元测试和关键发布缺口复查。
- 采纳“安全优先”的判定：现有静态验证不足以解除主控和电机相关风险，因此结论为暂不发布。
- 本次不修改功能代码；原因是本次任务是复审，且实机尺寸、模型和主控行为尚未获得可验证输入。

## 未采纳

- 未采纳“单元测试通过即可发布”的隐含判断；原因是测试未覆盖真实 MaixCAM、MSPM0、UART 物理链路和电机。
- 未采纳在未测量靶纸前写入推导得到的白框毫米尺寸；原因是会引入系统几何偏差。
- 未采纳立即切换二进制协议；原因是现有 MSPM0 固件未在本次审查范围内实现并验证，且 V2 首版约定保持 ASCII 兼容。

## 验证情况

- 已执行内存编译：工程内 68 个 Python 文件通过编译。
- 已执行：`python -B -m unittest discover -s tests -v`；48 项测试通过，耗时约 0.140 s。
- 未执行实机验证：没有连接 MaixCAM Pro、MSPM0G3507 或张大头步进电机；没有写入设备配置、没有发送真实串口数据、没有改动旧工程。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Skill template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-review-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\pipeline.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\main.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\config.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\timing.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\tools\pipeline_benchmark.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\laser\detector.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\laser\background.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\geometry\standard_plane.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\target\ai_recapture.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\protocol\mspm0_safety_reference.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-14_v2-release-readiness.md`
- Tool: `functions.exec -> shell_command`，命令 `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
- Tool: `functions.exec -> shell_command`，命令为 Python 内存编译脚本，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
