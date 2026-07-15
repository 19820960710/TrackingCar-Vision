# 2026-07-15 双轴 UART 云台调试阶段复盘

## 背景

- 项目使用 MSPM0G3507、两台 X42S 闭环步进电机和 MaixCAM 构建双轴视觉云台。
- 本阶段从“UART 无输出、只有一个轴运动、运行后卡住”推进到电脑连续轨迹驱动下两轴同时、双向、持续运行。
- 建立本目录是为了给后续 MCU 多 UART、视觉云台和协作式调度项目复用。

## 事实记录

- 最终硬件绑定为：Yaw 使用 PB6/PB7，Pitch 使用 PA23/PA24，视觉 UART 使用 PA0/PA1。
- PA23 曾因开发板两个 testpoint 连锡而与 3V3 短路；GPIO 高低翻转试验首先把问题定位到了硬件网络。
- 软件中的 Yaw/Pitch 实例曾与物理接线相反，导致观察到的轴与日志轴名不一致。
- Keil 启动文件最初只分配 256 B 栈；视觉解析、`snprintf`、诊断输出和中断叠加后越界，表现为运行一段时间后 LED、TV 和电机同时停止。
- 栈扩大为 4 KiB 后，100 Hz、250 Hz 和三轮连续仿真均完成；两轴 Q/F/A 对称，J/T/P/E 为零。
- 连续椭圆轨迹实机运行约 400 秒，用户观察到两轴持续同时运动；测试器随后按要求停止并释放 COM11。
- 当前 Keil 静态报告为 `Maximum Stack Usage = 404 bytes + Unknown`；`Unknown` 来自不可追踪函数指针或调用路径，未包含完整中断嵌套保证。

## 原因分析

- “两轴互相影响”不是最终根因。双轴独立 `poll/service_tx` 在相同调度周期内可以并行工作。
- 主要软件根因是栈空间不足；主要识别错误来源是轴绑定反置。
- PA23 无波形是独立的板级短路问题，不是 UART 初始化、上下拉或波特率问题。
- 上电自检与视觉接收互相干扰的问题通过“先完成双轴自检，再初始化并清空视觉 UART”隔离。
- 电脑脚本结束后再次 RESET 不能复现轨迹，是因为 RESET 只重启 MCU，不会重启电脑端数据源；主控没有内置轨迹发生器。

## 经典坑或可复用经验

- 先用 LED 心跳证明主循环活着，再判断通信逻辑。
- UART TX 无波形时，先把该引脚改为 1 Hz GPIO 翻转；GPIO 也失败就停止改 UART 代码，转查板级网络。
- 轴名、外设实例、引脚和物理电机必须用一张绑定表统一，不能靠“哪台电机动了”猜软件对象。
- 运行数秒或数百帧后随机停止，且增加日志后更快停止，应优先检查栈和内存越界。
- 调试器停住时 SysTick 不增长是正常现象；判断定时器必须让内核自由运行一段时间。
- 串口助手和 Python 不能同时独占同一 COM 口；一个程序占用 COM11 时，另一个程序看不到数据。
- 自动测试必须区分电机保护 `P` 和本地解析错误 `E`；`P` 需要立即停机，`E` 应记录原始帧并继续或按阈值处理。

## 流程调整

- 新项目按“供电与复位 → GPIO → 裸 UART 帧 → 单轴封装 → 双轴自检 → 返回帧 → 仿真视觉 → 真实视觉”推进，每步只改变一个变量。
- 所有运行期 UART 采用有限执行时间的状态机，不允许无限等待 FIFO、返回帧或固定 delay。
- 每个轴维护独立电机对象、TX 状态、RX 解析状态、计数器和更新时间。
- 所有压力测试同时记录 LED、TV、MS 和 Q/J/F/T/A/R/P/E，不能只看机械是否运动。
- 修改日志频率或加入 `snprintf` 后重新检查 Keil 栈报告；加入巡线、屏幕或 RTOS 前增加栈水位实测。

## 不纳入本次调整

- 尚未验证真实 MaixCAM 闭环的 Yaw/Pitch 物理正方向。
- 尚未测量 4 KiB 栈的运行期水位峰值。
- 尚未调优正式跟踪参数；当前 8 脉冲/像素、最大 400 脉冲是便于观察的 commissioning 参数。
- 尚未加入机械限位、累计角度软限位和目标丢失后的回中策略。

## 目录

- `failure-modes.md`：每一种“卡住/不动”现象、真实原因和判别方法。
- `future-project-checklist.md`：下次项目从零开始时的检查顺序。
- `maixcam-integration-preparation.md`：当前接入真实相机前的准备和验收步骤。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\review\2026-07-15_dual-axis-root-cause-review.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\change\2026-07-14_pa23-gpio-toggle-test.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\change\2026-07-14_nonblocking-uart-tx-with-timeout.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\change\2026-07-14_deferred-maixcam-uart-after-commissioning.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\keil\Objects\empty_LP_MSPM0G3507_nortos_keil.htm`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\test_logs\20260715-live-trajectory-retry\20260715-152908_100Hz_trajectory\summary.json`
- Source: 用户对 PA23 短路、自检和双轴机械运动的实机观察
- Tool: `functions.shell_command`，command `Get-CimInstance Win32_Process ... | Stop-Process`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
