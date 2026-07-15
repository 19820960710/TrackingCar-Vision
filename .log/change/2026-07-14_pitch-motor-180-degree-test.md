# 2026-07-14 Pitch 电机 180 度往返自检

## 修改目标

- 将 Pitch 电机单次自检从 20 脉冲的小幅抖动扩大为约 180 度往返，便于确认方向和完整位置运动。

## 修改内容

1. 将自检脉冲数从 20 改为 1600。
   - 按当前 X42S 封装的 3200 脉冲/圈假设，1600 脉冲约为半圈，即 180 度。
2. 每阶段等待改为 200,000,000 CPU cycles，约 2.5 秒。
   - 20 RPM 时半圈理论运动约 1.5 秒，等待时间足以避免正向未完成就下发反向命令。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`

## 验证情况

- 已用 TI Arm Clang 单独编译 `pitch_motor_control.c`，返回码为 0。
- 待实机验证机械行程、正反方向和回转角度。
- 实机预期：上电约 2.5 秒后，Pitch 正向约 180 度；约 2.5 秒后反向约 180 度；之后不再运动。

## 未处理事项

- 烧录前确认机械结构允许完整半圈运动，避免撞线束或机械限位。
- 脉冲/圈实际值仍需通过实机角度测量校准。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
