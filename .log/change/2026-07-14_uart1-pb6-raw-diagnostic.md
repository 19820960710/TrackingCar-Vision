# 2026-07-14 UART1 PB6 裸发送诊断

## 修改目标

- 在 PA23/UART2 监听未得到有效周期帧后，将临时诊断输出切换至已配置的 UART1_TX（PB6），区分 UART2 路径问题与板级串口监听问题。

## 修改内容

1. 修改 `main.c` 的临时裸 UART 诊断。
   - 不经过 `gimbal_motor` 封装、地址检查或电机运动状态机。
   - 每秒直接向 `stepMotor2_INST`（UART1/PB6）写入 `01 35 6B`。
2. 未改变 UART1 的 SysConfig 配置。
   - UART1/PB6 已配置为 MFCLK、115200 bit/s、8N1、TX/RX。
   - 电机自检保持关闭。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 已使用 TI ARM Clang 编译 `main.c`、`gimbal_motor.c` 与 `ti_msp_dl_config.c`，退出码为 0。
- 未完成实物验证。重新 Build、Download、RESET 后，使用 USB-TTL 的 RXD 连接 PB6、GND 共地、115200 bit/s HEX 显示，预期每秒收到 `01 35 6B`。

## 未处理事项

- 当前诊断输出不再由 PA23/UART2 发出；本轮不能据此判断 Yaw 电机的 UART2 路径。
- 验证结束后，需要恢复诊断输出至 UART2/PA23，再继续 Yaw 电机测试。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- Tool: `functions.apply_patch`，用于修改 `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
