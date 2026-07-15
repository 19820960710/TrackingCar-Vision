# 2026-07-15 进一步降低视觉跟踪转动幅度

## 修改目标

- 减小目标误差对应的电机相对位置指令，避免云台单次调整幅度过大。

## 修改内容

1. 将双轴共用的视觉跟踪比例从 `0.5 脉冲/像素` 降为 `0.2 脉冲/像素`。
2. 保持 4 像素死区、400 脉冲最大限幅、200 ms 控制周期和方向标志不变。

## 涉及文件

- `main.c`
- `.log/change/2026-07-15_further-reduce-tracking-amplitude.md`

## 验证情况

- Keil 全量构建结果为 `0 Error(s), 0 Warning(s)`。
- XDS110 烧录结果为 `Erase Done`、`Programming Done`、`Verify OK`。
- 实际跟踪幅度和收敛速度仍需连接 MaixCAM 实机观察。

## 未处理事项

- 未分别设置 Yaw/Pitch 比例；当前两轴继续共用同一比例。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: Keil UV4 命令行编译与烧录，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
