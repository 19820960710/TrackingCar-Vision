# 2026-07-14 UART 测试切换到 PB6

## 修改目标

- 将基于代码整理 ZDT 封装的 UART 测试发送口从 PA23/UART2 切换到 PB6/UART1。

## 修改内容

1. 将 `StepperMotor_init()` 的 UART 参数从 `stepMotor1_INST` 改为 `stepMotor2_INST`。
   - `stepMotor2_INST` 由生成配置映射为 UART1，TX 为 PB6，RX 为 PB7。
   - 不修改 `StepperMotor_enable()`、`StepperMotor_poll()`、波特率和 SysConfig UART 参数。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 已用 TI Arm Clang 编译修改后的 `main.c`，返回码为 0。
- 实机发送接线：电脑 RXD 接 PB6、GND 接开发板 GND；115200、8N1、HEX，预期每秒收到 `01 F3 AB 01 00 6B`。
- 实机接收接线：电脑 TXD 接 PB7；仅测试 ZDT 四字节响应帧接收。

## 未处理事项

- UART2/PA23 仍由 SysConfig 初始化，但主程序不使用它。
- 未修改电机地址和 ZDT 命令帧。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`
- Tool: `functions.exec`，`apply_patch`，cwd `C:\Users\Aupassen\Desktop\视觉`
