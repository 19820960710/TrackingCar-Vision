# 2026-07-14 Pitch 电机一次性运动自检

## 修改目标

- 验证 PB6/PB7 上 Pitch 步进电机的使能、正反方向和基础位置运动，不进入视觉控制。

## 修改内容

1. 新增 `pitch_motor_run_commissioning_test()`。
   - 电机使能后等待约 1 秒。
   - 发送正向 20 脉冲、20 RPM、加速度 10 的相对实时位置命令。
   - 等待约 1 秒后发送反向相同命令，再等待约 1 秒后返回。
2. 在 `main.c` 启用一次性自检开关。
   - `PITCH_MOTOR_COMMISSIONING_TEST_ENABLED` 为 1。
   - 自检调用只在启动阶段运行一次；随后主循环仅轮询电机响应，不再发送运动命令。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.h`

## 验证情况

- TI Arm Clang 已编译 `main.c`、`pitch_motor_control.c`、`pitch_tracker_control.c`、`zdt_x42s.c`、`stepper_motor.c`、`ti_msp_dl_config.c`，返回码均为 0。
- 待实机验证：下载后观察 Pitch 轴约 1 秒静止，再小幅正向、约 1 秒后小幅反向，之后不再运动。

## 未处理事项

- 正向与机械抬头/低头的对应关系尚未标定。
- 自检结束后仍保持使能，电机可能存在保持力；未发送失能命令。
- 视觉外环尚未接入本次测试。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- Tool: `functions.exec`，TI Arm Clang，cwd `C:\Users\Aupassen\Desktop\视觉`
