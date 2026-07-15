# 2026-07-14 延长视觉电机命令周期

## 修改目标

- 避免视觉闭环以 40 ms 周期重复覆盖尚未执行完成的 X42S 相对位置命令。

## 修改内容

1. 将两轴跟踪控制器的默认最短命令间隔从 40 ms 改为 200 ms。
   - 60 RPM、3200 脉冲/圈时，pitch 的 400 脉冲上限理论运行时间约为 125 ms；200 ms 间隔可让该动作在下一条命令前完成。
   - 两轴仍拥有独立的 `last_command_ms`，同一视觉帧中仍按 pitch、yaw 顺序发送，UART 帧格式不变。

## 涉及文件

- `pitch_tracker_control.c`

## 验证情况

- 使用 TI ARM Clang 对工程应用源文件执行编译检查。
- 未进行实机验证；需烧录后观察 `dy` 超过 ±4 时 pitch 是否动作，以及持续相反符号的 `dx` 是否使 yaw 反转。

## 未处理事项

- 底层 X42S 发送仍采用等待 TX FIFO 空位的短阻塞写；单个 13 字节帧在 115200 波特率下约需 1.13 ms。
- 尚未按电机“到位”应答门控下一条命令。本次仅改变周期，便于单独验证命令覆盖假设。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\zdt_x42s.c`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
