# 2026-07-15 多 UART 双轴项目复用检查表

## 背景

- 用于下一个 MSPM0、多 UART、电机或视觉项目的最小变量调试。
- 每一阶段通过后再进入下一阶段，失败时不跨层修改。

## 事实记录

### 1. 构建、下载与存活

- [ ] 确认实际烧录的工程路径和 AXF 时间戳。
- [ ] Build 为 0 error；记录 SysConfig 警告。
- [ ] 下载日志出现 Erase、Programming、Verify OK。
- [ ] 按 RESET 后 PB22 心跳持续翻转。
- [ ] 自由运行时 SysTick 单调增加。

### 2. 单引脚和单 UART

- [ ] TX 先做 1 Hz GPIO 高低翻转，实测 0 V/3.3 V。
- [ ] 测 TX 对 3V3/GND 是否异常短路。
- [ ] 恢复 UART 复用后发送固定 HEX 帧。
- [ ] 核对 115200、8N1、TTL 3.3 V、共地。
- [ ] 核对实例、IOMUX、TX/RX 方向和排针网络。

### 3. 单轴电机

- [ ] 只启用一台电机，使能、正转、反转分别验证。
- [ ] 记录发送完成 F、接受 A、保护 P、协议 E。
- [ ] 验证 `A=F`；若不等，先查返回线和解析器。
- [ ] 标记该物理轴对应的 UART 和正方向。

### 4. 双轴并行

- [ ] 每个轴使用独立对象、TX 缓冲、RX 状态和计数器。
- [ ] 两轴同时自检，LED 必须持续闪烁。
- [ ] Yaw-only、Pitch-only、Dual 正负误差分别测试。
- [ ] 100 Hz 通过后再做链路上限压力测试。
- [ ] 对比两轴 Q/J/F/T/A/P/E，而不只看机械运动。

### 5. 视觉接入

- [ ] 先用电脑串口仿真同一 AIM 协议和帧率。
- [ ] 自检结束后再初始化视觉 RX，并清空启动残留。
- [ ] 实机测量真实相机输出频率和最大行长度。
- [ ] 小幅标定 Yaw/Pitch 的物理反馈方向。
- [ ] 目标丢失时不得继续使用旧误差运动。

### 6. 稳定性

- [ ] 查看 Keil Maximum Stack Usage。
- [ ] 增加格式化日志、中断、巡线或屏幕后重新测栈水位。
- [ ] 主循环中不存在无限等待 FIFO、返回帧或固定 delay。
- [ ] 连续运行至少三轮；LED、TV、MS 均不中断。
- [ ] 正式版本降低 TV/MS 日志频率，并保留错误计数。

## 原因分析

- 检查表把硬件网络、外设配置、协议、调度和闭环方向分层，避免一次改动多个层级后无法定位。
- 计数器能把“命令没生成、没排队、没发完、没应答、机械没动”拆成不同故障层。

## 经典坑或可复用经验

- GPIO 试验比反复改 UART 初始化更快地区分板级问题。
- 方向标定必须以“误差是否减小”为标准，不以 CW/CCW 名称为标准。
- 压力测试通过不代表机械闭环方向正确；两项必须分开验收。

## 流程调整

- 新项目建立 `board_bindings.h` 或单一绑定表，禁止在多个文件重复写轴与 UART 对应关系。
- 自动测试输出机器可读 JSON，失败时保存最后计数和原始串口日志。

## 不纳入本次调整

- 本检查表不替代具体开发板原理图和电机协议手册。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\reflection\2026-07-15_dual-axis-debugging-reference\README.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\reflection\2026-07-15_dual-axis-debugging-reference\failure-modes.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_uart.c`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
