# 2026-07-14 统一双轴跟踪脉冲上限

## 修改目标

- 排查 PB6 通道不动作是否由 yaw 跟踪最大单次脉冲仅为80导致。

## 修改内容

1. 将 `GIMBAL_YAW_COMMISSIONING_MAXIMUM_PULSES` 从80改为400，与 pitch 跟踪上限一致。
   - 未改变 UART 绑定、方向、速度、加速度、死区、每像素脉冲数和命令周期。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 使用 TI ARM Clang 对 `main.c` 进行编译检查，退出码为0。
- 静态确认 yaw 与 pitch 的最大单次脉冲均为400。
- 尚未完成实机验证，需在 Keil 中重新 Build、Download、复位后观察 PB6 通道是否开始动作。

## 未处理事项

- 当前仍保留上一轮接口交换诊断映射，未在本次恢复最终物理轴映射。
- 本次未让跟踪命令采用自检的20 RPM、加速度10、固定1600脉冲和2500 ms周期；若PB6仍不动作，再单独进行该实验。
- 未处理运行一段时间后主循环停止的问题。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\change\2026-07-14_swap-axis-uart-binding-diagnostic.md`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.shell_command`，command `tiarmclang.exe -mcpu=cortex-m0plus -D__MSPM0G3507__ ... -c main.c`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
