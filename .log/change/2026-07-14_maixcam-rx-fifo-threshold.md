# 2026-07-14 MaixCAM UART 接收 FIFO 阈值调整

## 修改目标

- 缓解 MaixCAM 连续发送 AIM 帧时 UART0 每字节中断导致主循环处理不及时、软件环形缓冲溢出的情况。

## 修改内容

1. 将 UART0 RX FIFO 中断阈值从 1 字节改为半满。
2. 保留 RX timeout 中断，使不足半 FIFO 的一帧尾部仍能及时进入接收 ISR。
3. 未改变 UART0 的 PA1 RX、PA0 TX、115200 baud，以及 AIM 解析与 VU 诊断逻辑。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.c`

## 验证情况

- SysConfig 生成代码确认 UART0 使用 `DL_UART_RX_FIFO_LEVEL_1_2_FULL`。
- 使用 TI Arm Clang 编译主程序、生成配置、步进电机和视觉模块源文件，编译通过。
- 未实机验证。烧录后预期 VU 第三字段（累计接收溢出）不再快速增长，第一字段（成功解析帧数）持续增长。

## 未处理事项

- 当前 VU 文本在 TTL 端出现字段回退/缺字现象，主控内统计值按代码只能递增；该现象需以本次阈值调整后的新输出复测，并检查 PA0→TTL RX 物理连接和串口工具设置。
- 双电机上电自检仍保持启用，不受本次 UART0 FIFO 设置影响。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_uart.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_config.h`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- Tool: `functions.exec`，命令 `C:\ti\sysconfig_1.21.0\sysconfig_cli.bat ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.exec`，命令 `C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
