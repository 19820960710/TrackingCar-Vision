# 2026-07-14 Yaw单轴视觉闭环诊断

## 修改目标

- 在双轴启用时出现pitch动作而yaw不动作后，仅关闭pitch跟踪，验证PA23/UART2上的yaw闭环能否独立运行。

## 修改内容

1. 将`PITCH_TRACKING_ENABLED`从`1U`改为`0U`。
   - `YAW_TRACKING_ENABLED`保持`1U`。
   - 未改变UART绑定、方向、死区、比例、脉冲上限、速度、加速度、发送周期和发送服务顺序。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 用户提供的TV数据中，除一帧`dx=-2`外，其余`dx=10~32`，均超过4像素死区；按每像素8脉冲计算，yaw命令应为80~256脉冲。
- 使用TI ARM Clang对`main.c`进行编译检查，退出码为0。
- 静态确认yaw跟踪开关为`1U`、pitch跟踪开关为`0U`。
- 尚未完成实机验证。

## 未处理事项

- 若yaw单轴能动作，可确认两条轴链路单独正常，后续应检查双轴同帧命令提交和发送状态，而不是继续更换硬件或调整死区。
- 若yaw单轴仍不动作，需要继续检查PA23/UART2当前绑定及yaw命令提交状态。
- 未处理运行一段时间后主循环停止的问题。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\change\2026-07-14_reenable-yaw-dual-axis-ab-test.md`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.shell_command`，command `tiarmclang.exe -mcpu=cortex-m0plus -D__MSPM0G3507__ ... -c main.c`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
