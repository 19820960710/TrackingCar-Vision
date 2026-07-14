# 2026-07-14 UART 测试再次切到 PB6

## 修改目标

- 将 UART 测试发送口再次切换到 PB6/UART1，便于与 PA23/UART2 的实机结果对照。

## 修改内容

1. 将 `StepperMotor_init()` 的 UART 参数从 `stepMotor1_INST` 改为 `stepMotor2_INST`。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 已用 TI Arm Clang 编译修改后的 `main.c`，返回码为 0。
- 电脑 RXD 接 PB6、GND 接开发板 GND；115200、8N1、HEX，预期每秒收到 `01 F3 AB 01 00 6B`。

## 未处理事项

- 本次未修改 UART 参数、封装发送逻辑或接收处理。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Tool: `functions.exec`，`apply_patch`，cwd `C:\Users\Aupassen\Desktop\视觉`
