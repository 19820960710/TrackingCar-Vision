# 2026-07-15 分轴实测后修正 Yaw 方向

## 修改目标

- 根据最新分轴实测，只反转错误的 Yaw 跟踪方向，保持正确的 Pitch 方向。

## 修改内容

1. 将 Yaw 的 `positive_error_is_cw` 从 `true` 改为 `false`。
2. 保持 Pitch 的 `positive_error_is_cw` 为 `true`。
3. 未修改轴绑定：PB6/PB7 仍为 Yaw，PA23/PA24 仍为 Pitch。

## 涉及文件

- `main.c`
- `.log/change/2026-07-15_correct-yaw-direction-after-axis-test.md`

## 验证情况

- Keil 全量编译结果为 `0 Error(s), 0 Warning(s)`。
- XDS110 烧录结果为 `Erase Done`、`Programming Done`、`Verify OK`。
- Yaw 最终方向需实机观察 `dx` 是否随转动收敛。

## 未处理事项

- 无。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: Keil UV4 全量编译与烧录，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
