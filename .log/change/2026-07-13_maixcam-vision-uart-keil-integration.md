# 2026-07-13 MaixCAM 视觉 UART Keil 例程集成

## 修改目标

- 在不修改 `C:\Users\Aupassen\Desktop\empty_routine` 的前提下，创建可在 Keil 中构建并烧录的 MaixCAM UART 接收例程。

## 修改内容

1. 创建副本 `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`。
2. 将 `vision_comm` 复制到副本，并将三个生产 `.c` 文件加入 Keil 的 `VisionComm` 文件组。
3. 修改 `empty.syscfg`，生成 UART3：PB2 TX、PB3 RX、115200 baud、FIFO、RX/RX timeout 中断。
4. 修改 `main.c`，增加 SysTick 1 ms 时间基、UART3 ISR、通信处理和 PA14 LED 首次验证指示。
5. 新增烧录与接线说明 `README_VISION_UART.md`。

## 验证情况

- SysConfig CLI 已成功生成 `ti_msp_dl_config.c/.h`，生成宏为 `UART_3_INST`、`UART_3_INST_INT_IRQN`、`UART_3_INST_IRQHandler`。
- 使用 TI Arm Clang 4.0.2.LTS 编译 `main.c`、三个通信源文件和 SysConfig 源文件成功。
- 编译时 DriverLib 头文件报告 3 个既有 unused-parameter 警告；通信与应用代码没有诊断。
- 未找到本机 Keil `UV4.exe` 命令行程序，未在此环境运行 Keil 最终链接或下载。
- 未进行 MaixCAM/MSPM0 真机通信验证。

## 未处理事项

- 需在实际 Keil 中 Build 并按板载下载器配置烧录。
- PA14 LED 的有效电平需按实际板卡确认。
- 本例程不包含 X42S 电机控制、云台控制环、零位或限位保护。

## 依据与工具

- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\vision_comm\`
- Source: `C:\Users\Aupassen\Desktop\empty_routine\empty.syscfg`
- Tool: `functions.shell_command`, command `Copy-Item` and `sysconfig_cli.bat`, cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.apply_patch`, cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
