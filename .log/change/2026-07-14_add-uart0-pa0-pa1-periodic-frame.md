# 2026-07-14 新增 UART0 PA0/PA1 周期发送

## 修改目标

- 新增 PA0/PA1 上的独立 UART0，与当前 PB6/UART1 同时发送相同的 ZDT X42S 使能帧。

## 修改内容

1. 在 `empty.syscfg` 新增 `pcUart`。
   - 外设：UART0；TX：PA0；RX：PA1；时钟：MFCLK；波特率：115200；8N1；FIFO 使能。
2. 重新生成 `ti_msp_dl_config.c` 和 `ti_msp_dl_config.h`。
   - 生成宏 `pcUart_INST = UART0`、TX 为 PA0、RX 为 PA1。
3. 在 `main.c` 增加第二个 `StepperMotor` 上下文。
   - 当前 PB6/UART1 和新增 PA0/UART0 都在每个周期调用同一份 `StepperMotor_enable()`，各发送 `01 F3 AB 01 00 6B`。
   - 两个 UART 都调用原有 `StepperMotor_poll()` 接收 ZDT 四字节响应。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- SysConfig 1.21.0 生成成功，除 TI Flash 状态位提示外无错误。
- TI Arm Clang 已编译 `main.c`、`zdt_x42s.c`、`stepper_motor.c`、`ti_msp_dl_config.c`，返回码为 0。
- 待实机验证：电脑 RXD 接 PA0、GND 接开发板 GND，115200/8N1/HEX；应每约一秒收到 `01 F3 AB 01 00 6B`。PB6 仍会在同一周期发送同一帧。

## 未处理事项

- 未新增通用电脑文本回显；接收仍严格使用代码整理中的 ZDT 四字节响应轮询。
- 同时接两个 USB-TTL 时必须共地，且各自 RXD 只连接对应 MCU TX。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\source\ti\devices\msp\m0p\mspm0g350x.h`
- Source: `C:\Users\Aupassen\Desktop\代码整理\drivers\stepper_motor\zdt_x42s\stepper_motor.c`
- Tool: `functions.exec`，SysConfig CLI、TI Arm Clang，cwd `C:\Users\Aupassen\Desktop\视觉`
