# 2026-07-15 核对轴映射后反转双轴跟踪方向

## 修改目标

- 在确认 PB6/PB7 对应 Yaw、PA23/PA24 对应 Pitch 后，根据实机结果反转两个轴的视觉闭环方向。

## 修改内容

1. 核对 `stepMotor1` 为 UART1、PB6 TX/PB7 RX，并绑定 `g_yaw_motor`。
2. 核对 `stepMotor2` 为 UART2、PA23 TX/PA24 RX，并绑定 `g_pitch_motor`。
3. 核对 `dx` 送入 Yaw 控制器、`dy` 送入 Pitch 控制器。
4. 将 Yaw 和 Pitch 的 `positive_error_is_cw` 配置同时由 `false` 改为 `true`。
5. 保持 `0.2 脉冲/像素`、死区、限幅、自检和调度逻辑不变。

## 涉及文件

- `main.c`
- `ti_msp_dl_config.h`（只读核对）
- `pitch_tracker_control.c`（只读核对）
- `.log/change/2026-07-15_invert-both-directions-after-axis-check.md`

## 验证情况

- Keil 全量编译结果为 `0 Error(s), 0 Warning(s)`，并确认重新编译了 `main.c`。
- XDS110 烧录结果为 `Erase Done`、`Programming Done`、`Verify OK`。
- 最终方向仍需实机确认目标偏离时 `|dx|`、`|dy|` 是否减小。

## 未处理事项

- 未修改开机自检方向，因为本次只校准视觉闭环。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`
- Tool: `functions.shell_command`，轴绑定和方向链路检查，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: Keil UV4 全量编译与烧录，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
