# 2026-07-14 交换双轴 UART 绑定诊断

## 修改目标

- 在已确认故障跟随 PB6/PA23 板端通道、而非跟随物理电机后，仅交换 yaw 与 pitch 的软件 UART 绑定，区分故障属于逻辑控制链还是 UART1/PB6 通道。

## 修改内容

1. 将 `g_pitch_motor` 从 `stepMotor1_INST`（UART1/PB6）改为 `stepMotor2_INST`（UART2/PA23）。
2. 将 `g_yaw_motor` 从 `stepMotor2_INST`（UART2/PA23）改为 `stepMotor1_INST`（UART1/PB6）。
3. 更新 `main.c` 中的诊断映射注释。
   - 未改变 `dx/dy` 分配、方向、速度、加速度、脉冲限制、协议帧和发送服务顺序。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 使用 TI ARM Clang 对 `main.c` 进行编译检查，退出码为 0。
- 尚未完成实机验证。
- 实机前提：保持用户当前交换后的物理接线不变，即 PA23 接物理 pitch 电机、PB6 接物理 yaw 电机。

## 未处理事项

- 本次不修改两个 UART 的 SysConfig 参数和引脚复用。
- 本次不增加诊断输出，避免引入第二个行为变量。
- 本次不处理运行一段时间后主循环停止的问题。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\change\2026-07-14_swap-motor-tx-service-order.md`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.shell_command`，command `tiarmclang.exe -mcpu=cortex-m0plus -D__MSPM0G3507__ ... -c main.c`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
