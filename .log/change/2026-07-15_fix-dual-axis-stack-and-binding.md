# 2026-07-15 双轴并行控制修复

## 修改目标

- 按实际接线统一 Yaw、Pitch 与两路电机 UART 的软件绑定。
- 消除视觉回传与电机诊断运行一段时间后主循环停止的问题。
- 保留非阻塞的双轴独立发送、接收和轮询结构。

## 修改内容

1. 修正轴与 UART 实例的绑定。
   - Yaw 使用 `stepMotor1_INST`，对应 PB6/PB7。
   - Pitch 使用 `stepMotor2_INST`，对应 PA23/PA24。
   - 两个 commissioning 状态机和 tracker 均跟随各自物理轴。
2. 将 Keil 启动文件中的栈空间由 `0x100`（256 B）扩大为 `0x1000`（4 KiB）。
   - Keil 静态调用图报告给出的已知最大栈需求至少为 348 B，且仍包含未知的间接调用和中断开销。
   - 原配置不足以承载视觉 `TV`、电机 `MS` 的 `snprintf` 调用与中断嵌套。
3. 保持两轴主循环非阻塞。
   - 每轮分别调用两轴 `poll` 和 `service_tx`。
   - 任一轴的发送或应答状态不会等待、阻塞另一轴。
4. 保留并重新启用 `MOTOR_DIAGNOSTIC_ENABLED`，用于输出两轴独立的 Q/J/F/T/A/R/P/E 计数。
5. 完善电脑端 MaixCAM 仿真脚本。
   - 支持 100 Hz、250 Hz、单轮或多轮场景。
   - 分阶段发送 Yaw-only、Pitch-only、Dual 正负误差。
   - 保存原始串口输出和 JSON 统计。
   - 忽略复位瞬间位于行首的非打印启动噪声，使 `VISION_READY` 能被稳定识别。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\keil\startup_mspm0g350x_uvision.s`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\maixcam_uart_simulator.py`

## 验证结果

- Keil Target 编译：0 errors，0 warnings。
- XDS110 下载并 Verify：通过。
- 100 Hz 单轮：完成，2092 条 TV、49 条 MS，双轴最终均为 `Q=42,J=0,F=43,T=0,A=43,R=0,P=0,E=0`。
- 250 Hz 单轮：完成，5229 条 TV、400 条 MS，双轴最终计数仍相同，未发生停止。
- 100 Hz 三轮验收：完成，6070 条 TV、178 条 MS；双轴最终均为 `Q=122,J=0,F=123,T=0,A=123,R=0,P=0,E=0`。
- 实机观察确认：最终 Dual 阶段 Yaw 与 Pitch 同时旋转。

`F` 比 `Q` 多 1 是初始化使能帧产生的固定差值，不是丢包。`A=F` 表明发送完成的帧均收到电机接受应答。

## 测试产物

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\test_logs\20260715-stack-4k-ms-enabled-100Hz\20260715-144853_100Hz\summary.json`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\test_logs\20260715-stack-4k-ms-enabled-250Hz\20260715-145157_250Hz\summary.json`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\test_logs\20260715-final-3cycles-100Hz\20260715-145659_100Hz\summary.json`

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Tool: Keil µVision command-line Build
- Tool: XDS110 Flash/Verify
- Tool: Python `pyserial` 串口仿真与统计
