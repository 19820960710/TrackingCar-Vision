# 2026-07-14 MaixCAM UART0 接收与回显联调

## 修改目标

- 使用 MSPM0 的 UART0（PA0/PA1）接收 MaixCAM 的目标坐标帧，并从 PA0 回显规范化的 `TV` 帧，供串口助手确认通信。

## 修改内容

1. MaixCAM 视觉配置启用串口输出，输出模式设为 `target`。
   - `/dev/ttyS1`、A19/A18、115200 保持不变。
   - 视觉帧率为 60 FPS 且每 2 帧发送一次，理论发送频率约 30 Hz。
2. SysConfig 新增 `maixcam` UART0。
   - PA0 复用为 UART0_TX，PA1 复用为 UART0_RX，115200 bps，FIFO 使能。
3. 使 `vision_uart_init()` 显式开启 UART RX 中断。
4. 主程序初始化 MaixCAM 接收模块和 SysTick 毫秒时基。
   - `UART0_IRQHandler()` 只调用接收模块的中断入口。
   - 成功解析一帧后，从 UART0_TX 回显 `TV,valid,dx,dy,x,y`。
   - 本阶段仅验证通信；未调用 Pitch 跟踪控制，且关闭 180 度电机自检。
5. Keil 工程新增 `vision_observation.c`、`vision_packet.c`、`vision_uart.c`。

## 涉及文件

- `C:\Users\Aupassen\Desktop\视觉\maixcam\config.py`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_uart.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\keil\empty_LP_MSPM0G3507_nortos_keil.uvprojx`

## 验证情况

- SysConfig CLI 已成功生成 PA0/PA1 的 UART0 配置，返回码为 0。
- 使用 TI Arm Clang 编译主程序、电机模块、视觉通信模块和 SysConfig 生成文件，返回码为 0。
- 尚未进行 MaixCAM、MSPM0 和 USB-TTL 三者实机联调。

## 未处理事项

- 串口助手应接 MSP PA0（TX）而不是 PA1（RX）；USB-TTL RX 与 PA0 相连，三方 GND 共地。
- MaixCAM A19（TX）接 MSP PA1（RX）。本测试不需要 MaixCAM A18（RX）。
- 若 MaixCAM 未以 60 FPS 运行、目标检测耗时升高或丢帧，实际发送频率会低于约 30 Hz。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\maixcam\drivers\uart_output.py`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_uart.c`
- Tool: `functions.exec`，SysConfig CLI 和 TI Arm Clang，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
