# 2026-07-15 翻转两轴视觉反馈方向

## 修改目标

- 修正真实 MaixCAM 闭环测试中 Yaw、Pitch 均使目标误差增大的物理反馈方向。

## 修改内容

1. Yaw 的 `positive_error_is_cw` 从 `false` 改为 `true`。
2. Pitch 的 `positive_error_is_cw` 从 `true` 改为 `false`。
3. 保持0.5脉冲/像素、4像素死区、200 ms命令周期、速度、加速度和30度开机自检不变。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- Keil Target 编译通过：`0 Error(s), 0 Warning(s)`；SysConfig 仍有一条既有的 Flash 状态最佳实践提示。
- XDS110 下载日志显示 `Erase Done`、`Programming Done`、`Verify OK`。
- 烧录后需实机确认：目标在画面右侧/下方时，第一次修正后 `|dx|`/`|dy|` 应减小。

## 未处理事项

- 本次只修正方向，不调节比例或死区；若方向正确但仍振荡，再单独调节比例和命令周期。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: 用户对真实相机闭环中 Yaw、Pitch 两轴方向均相反的实机观察
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
