# 2026-07-14 yaw 方向反转与安全校准限幅

## 修改目标

- 处理 yaw 视觉闭环连续向同一方向旋转、接近机械行程后停止的现象。
- 在未完成方向校准前降低 yaw 单条闭环命令的机械风险。

## 修改内容

1. 将 `GIMBAL_YAW_POSITIVE_ERROR_IS_CW` 设为 `false`。
   - 水平像素误差对应的 yaw 转向与上一版完全相反，用于验证原先是否为正反馈。
2. 将 yaw 闭环的单条最大脉冲独立限制为 80。
   - pitch 的最大脉冲仍保持 400，本次不改变其方向、误差计算或驱动通道。

## 涉及文件

- `main.c`

## 验证情况

- 使用 TI ARM Clang 对工程应用源文件执行编译检查。
- 未进行实机验证。烧录后应从安全位置开始，只观察目标左右偏离时 yaw 是否向画面中心收敛；若仍跑离目标，应立即断电并报告 `TV` 中的 `dx`。

## 未处理事项

- pitch 自检正常但视觉跟踪无动作的问题未在本次修改。需先确认 `TV` 的 `dy` 是否超过 ±4 像素。
- 本工程当前是视觉像素 P 控制，不含机械限位开关、回零和独立电机位置 PID；调试阶段应保留足够机械余量。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
