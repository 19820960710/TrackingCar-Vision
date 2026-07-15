# 2026-07-15 修正视觉坐标范围为 320×240

## 修改目标

- 将主控视觉参考中心从错误的 `(256,160)` 修正为实际 320×240 坐标系中心 `(160,120)`。

## 修改内容

1. 将默认视觉帧宽高从 512×320 改为 320×240。
2. `main.c` 改为引用 `vision_config.h` 中的统一宽高配置，不再重复硬编码。
3. 未修改双轴方向、`0.2 脉冲/像素`、停止逻辑、速度和控制周期。

## 涉及文件

- `main.c`
- `vision_comm/vision_config.h`
- `.log/change/2026-07-15_correct-vision-frame-to-320x240.md`

## 验证情况

- Keil 全量编译结果为 `0 Error(s), 0 Warning(s)`。
- XDS110 烧录结果为 `Erase Done`、`Programming Done`、`Verify OK`。
- 需要实机确认目标是否收敛到 `(160,120)` 附近。

## 未处理事项

- 尚未加入 PID/PD；应先验证正确参考中心下的纯比例控制表现。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_config.h`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: Keil UV4 全量编译与烧录，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
