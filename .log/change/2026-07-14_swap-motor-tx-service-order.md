# 2026-07-14 交换双轴 UART 发送服务顺序

## 修改目标

- 仅改变 yaw 与 pitch 两路电机发送状态机在主循环中的服务先后顺序，用于判断 pitch 不动作是否由调度顺序或共享资源导致。

## 修改内容

1. 将主循环中的发送服务顺序从 `pitch -> yaw` 改为 `yaw -> pitch`。
   - 未改变跟踪器与电机对象的绑定。
   - 未改变 `dx/dy` 分配、方向、速度、加速度、脉冲限制和 UART 引脚配置。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 使用 TI ARM Clang 对 `main.c` 进行编译检查，退出码为 0。
- 尚未完成实机验证。需要在 Keil 中重新 Build、Download、复位后观察：仍只有 yaw 动、改为只有 pitch 动，或两轴行为均无变化。

## 未处理事项

- 本次故意不交换 UART 接口，避免同时改变多个变量。
- 本次不处理运行一段时间后整机停止的问题。
- 若故障不随发送顺序转移，下一项实验再单独交换两轴的 UART 对象绑定。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\stepper_motor.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\zdt_x42s.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.shell_command`，command `tiarmclang.exe -mcpu=cortex-m0plus -D__MSPM0G3507__ ... -c main.c`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
