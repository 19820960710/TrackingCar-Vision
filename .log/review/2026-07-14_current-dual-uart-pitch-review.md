# 2026-07-14 当前双 UART Pitch 工程审查

## 检查范围

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.*`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.*`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\keil\empty_LP_MSPM0G3507_nortos_keil.uvprojx`

## 检查依据

- 当前目标为 PA23/UART2 与 PB6/UART1 的双路电机命令测试。
- 生成配置、主程序实例和 Keil 编译文件必须一致。

## 发现

1. 双路 UART 的初始化、引脚复用和主程序实例一致。
   - 影响：代码层面不存在“UART2 未初始化”或“PA23 未配置为 UART2_TX”的问题。
   - 证据：`empty.syscfg:43-58` 配置 UART2/PA23-PA24，`empty.syscfg:60-75` 配置 UART1/PB6-PB7；`ti_msp_dl_config.c:53-54` 初始化两路，`ti_msp_dl_config.c:76-84` 配置两组外设引脚；`main.c:13-16` 初始化并使能两路实例。
   - 处理：无需修改；仍需要实机验证 PA23 的板级电气条件。

2. 当前程序不会周期发送命令。
   - 影响：启动时只对每台电机发送一次使能帧；之后主循环仅轮询接收。若串口助手在运行后才连接，或期待每秒持续收到数据，将看不到新的发送帧。
   - 证据：`main.c:15-16` 是唯一 `pitch_motor_enable()` 调用；`PITCH_MOTOR_SELF_TEST_ENABLED` 在 `main.c:5` 为 0，循环 `main.c:18-26` 中没有发送调用。
   - 处理：需要实机验证后再决定是否增加独立的周期诊断发送，不建议把使能命令无限重复当作正常控制。

3. 自检函数在双电机情况下共用方向状态，开启后两台电机方向不一致。
   - 影响：若将 `PITCH_MOTOR_SELF_TEST_ENABLED` 设为 1，第一次循环中 PA23 对应电机发送正向，PB6 对应电机发送反向；这不符合“两个口发送同一动作”的测试含义。
   - 证据：`pitch_motor_control.c:56` 的 `static bool positive` 在函数内跨所有电机上下文共享；`main.c:23-24` 连续调用同一函数两次。
   - 处理：需要修改后再开启双电机自检；当前自检关闭，未影响现有运行。

4. 模块命名尚未反映实际轴归属。
   - 影响：`g_yaw_motor` 使用 PA23/UART2，`g_pitch_motor` 使用 PB6/UART1；如果 PA23 已弃用，名称会增加后续视觉接入时的误接风险。
   - 证据：`main.c:7-8`。
   - 处理：暂不修改，待确定最终是否保留 PA23 后统一命名。

## 已采纳

- 本次仅审查，不修改现有代码。

## 未采纳

- 不在未确认机械行程时开启自检或自动位置移动。
- 不根据代码审查结论推断 PA23 的硬件电气状态；需要实机测量或原理图确认。

## 验证情况

- TI Arm Clang 已编译 `main.c`、`pitch_motor_control.c`、`zdt_x42s.c`、`stepper_motor.c`、`ti_msp_dl_config.c`，返回码均为 0。
- 未进行实机测量。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- Tool: `functions.exec`，PowerShell `Get-Content`、`Select-String`、TI Arm Clang，cwd `C:\Users\Aupassen\Desktop\视觉`
