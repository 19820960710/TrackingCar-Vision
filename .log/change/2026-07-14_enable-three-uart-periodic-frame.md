# 2026-07-14 三路 UART 周期发送

## 修改目标

- 让 PA23/UART2、PB6/UART1、PA0/UART0 都发送同一份 ZDT 帧，便于分别接入串口助手比较。

## 修改内容

1. 在 `main.c` 新增第三个 `StepperMotor` 上下文，绑定 `stepMotor1_INST`。
   - `stepMotor1_INST` 为 UART2，TX 为 PA23，RX 为 PA24。
2. 每个周期依次调用三次原有 `StepperMotor_enable()`。
   - PB6、PA0、PA23 都发送 `01 F3 AB 01 00 6B`。
3. 对三路 UART 均调用原有 `StepperMotor_poll()`。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 已用 TI Arm Clang 编译修改后的 `main.c`，返回码为 0。
- 三路 TX：PB6、PA0、PA23；均为 115200、8N1，约每秒发送一次相同帧。

## 未处理事项

- 三个帧在同一主循环中顺序发送，存在微秒级先后，不是硬件级的严格同一时刻。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`
- Tool: `functions.exec`，`apply_patch`，cwd `C:\Users\Aupassen\Desktop\视觉`
