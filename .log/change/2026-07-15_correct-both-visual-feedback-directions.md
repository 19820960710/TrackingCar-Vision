# 2026-07-15 校准视觉闭环方向

## 修改目标

- 根据实机分轴观察结果，保持正确的 Yaw 方向，只反转 Pitch 的视觉闭环控制方向。

## 修改内容

1. 保持 Yaw 正误差对应方向为 CCW。
2. 将 Pitch 正误差对应方向由 CW 改为 CCW。
3. 未修改开机自检、脉冲比例、死区、串口和非阻塞调度逻辑。

## 涉及文件

- `main.c`
- `.log/change/2026-07-15_correct-both-visual-feedback-directions.md`

## 验证情况

- 已执行 Keil 全量编译，结果为 `0 Error(s), 0 Warning(s)`。
- 已通过 XDS110 烧录，结果为 `Erase Done`、`Programming Done`、`Verify OK`。
- 视觉反馈方向仍需实机观察目标偏移后误差是否收敛。

## 未处理事项

- 未修改电机驱动器内部状态；烧录后需按 RESET，并在必要时对电机驱动器完整断电重启。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: Keil UV4 命令行编译与烧录，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
