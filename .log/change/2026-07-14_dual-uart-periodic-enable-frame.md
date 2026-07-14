# 2026-07-14 双 UART 周期使能帧

## 修改目标

- 让 PA23/UART2 与 PB6/UART1 在运行期间持续发送同一份 UART 测试帧，方便用串口助手同时检查两个 TX 引脚。

## 修改内容

1. 在主循环中按 PB6、PA23 顺序分别调用 `pitch_motor_enable()`。
   - 两路均通过代码整理的 `StepperMotor_enable()` 发送 `01 F3 AB 01 00 6B`。
2. 在每轮循环末尾增加约 1 秒延时。
   - 两个 TX 引脚每约一秒各发送一次；两帧在主循环中顺序出现。
3. 不调用位置移动接口。
   - 本次只验证 UART TX，不命令电机发生位置移动。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 已用 TI Arm Clang 编译修改后的 `main.c`，返回码为 0。
- 串口助手接 PA23 或 PB6，115200/8N1/HEX，应周期收到 `01 F3 AB 01 00 6B`。

## 未处理事项

- PA23 是 VREF+ 复用脚，周期帧若仍不可见，需要按板级电气问题处理。
- 自检仍关闭。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\stepper_motor.c`
- Tool: `functions.exec`，`apply_patch`，cwd `C:\Users\Aupassen\Desktop\视觉`
