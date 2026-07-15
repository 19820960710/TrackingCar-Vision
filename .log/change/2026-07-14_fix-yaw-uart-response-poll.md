# 2026-07-14 修正 yaw 电机 UART 应答轮询

## 修改目标

- 修正 PA23/PA24（yaw）电机通道在主循环中未被轮询的问题，避免其 RX FIFO 中的电机应答长期不被读取。

## 修改内容

1. 将 `main.c` 主循环中的第二次 `pitch_motor_poll(&g_pitch_motor)` 改为 `pitch_motor_poll(&g_yaw_motor)`。
   - PB6/PB7 仍为 pitch；PA23/PA24 仍为 yaw。
   - 未改变视觉闭环参数、UART 波特率、命令格式或开机自检状态机。

## 涉及文件

- `main.c`

## 验证情况

- 使用 TI ARM Clang 对工程全部应用源文件执行编译检查，预期无编译错误。
- 尚未在实机验证；需要烧录后观察两轴开机自检和视觉偏移时的响应。

## 未处理事项

- 当前闭环测试参数为死区 4 像素、比例 8 脉冲/像素、单次上限 400 脉冲。若两轴轮询修正后仍有运行一段时间停止的问题，再单独降低命令频率；本次不混入该改动。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\zdt_x42s.c`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
