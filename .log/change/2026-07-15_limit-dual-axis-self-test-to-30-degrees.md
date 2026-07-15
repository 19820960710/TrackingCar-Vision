# 2026-07-15 双轴开机自检限制为正负30度

## 修改目标

- 摄像机安装到云台后，避免开机自检执行大角度旋转，降低线缆缠绕和机械碰撞风险。

## 修改内容

1. 将两轴共用的自检行程从固定 1600 脉冲改为按 30 度计算。
2. 使用 X42S 配置的 3200 脉冲/圈进行四舍五入：`(3200×30+180)/360=267` 脉冲，对应约 30.04 度。
3. 自检状态机保持不变：两轴先正向约30度，再反向约30度回到起始位置。
4. 不修改自检速度、加速度、等待时间和两轴方向。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`

## 验证情况

- Keil Target 编译通过：`0 Error(s), 0 Warning(s)`；SysConfig 仍有一条既有的 Flash 状态最佳实践提示。
- XDS110 下载日志显示 `Erase Done`、`Programming Done`、`Verify OK`。
- 烧录后仍需按板上 RESET 才能可靠启动当前固件；需要实机观察两轴行程和线缆余量。

## 未处理事项

- 30度是电机轴按3200脉冲/圈换算的角度；若机械结构存在额外减速比，云台实际角度需要按传动比重新标定。
- 本次不修改此前已降低的视觉跟踪比例。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\zdt_x42s.h`
- Source: 用户对已安装摄像机后不能执行大角度自检的机械安全要求
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
