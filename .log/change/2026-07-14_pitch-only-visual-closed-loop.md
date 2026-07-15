# 2026-07-14 Pitch单轴视觉闭环诊断

## 修改目标

- 将双轴视觉跟踪拆成独立轴开关，先只运行PB6/UART1上的pitch闭环，排除yaw控制与PA23发送对诊断的干扰。

## 修改内容

1. 恢复与当前物理接线一致的软件绑定。
   - `g_pitch_motor`绑定`stepMotor1_INST`（UART1，PB6/PB7）。
   - `g_yaw_motor`绑定`stepMotor2_INST`（UART2，PA23/PA24）。
2. 增加独立编译开关。
   - `PITCH_TRACKING_ENABLED = 1U`。
   - `YAW_TRACKING_ENABLED = 0U`。
3. 视觉阶段仅执行`dy -> g_pitch_tracker -> g_pitch_motor -> PB6`。
   - 仍计算并回传`dx/dy`，便于观察输入数据。
   - 保留双轴开机自检；yaw只参与自检，不参与视觉跟踪。
4. 保留双轴相同的400脉冲上限、60 RPM、加速度20、4像素死区和200 ms命令周期。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 使用TI ARM Clang对`main.c`进行编译检查，退出码为0。
- 静态确认pitch绑定UART1/PB6，yaw绑定UART2/PA23。
- 静态确认视觉处理只调用`g_pitch_tracker`。
- 尚未完成实机验证。需在Keil中重新Build、Download、复位后，使用明显的垂直偏差测试PB6上的pitch电机。

## 未处理事项

- 本次未把跟踪参数改为自检参数；若pitch仍不动作，下一实验再让视觉启动后的PB6发送20 RPM、加速度10、固定1600脉冲、2500 ms周期的命令。
- 本次未处理运行一段时间后主循环停止的问题。
- yaw闭环尚未单独验收，当前由编译开关关闭。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\change\2026-07-14_equalize-axis-tracking-pulse-limit.md`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.shell_command`，command `tiarmclang.exe -mcpu=cortex-m0plus -D__MSPM0G3507__ ... -c main.c`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
