# 2026-07-14 MaixCAM PA1 AIM 接收验证配置

## 修改目标

- 使 UART0 的 PA1 接收端能及时接收 MaixCAM/TTL 发送的换行分隔 AIM 文本帧，并保留 PA0 回传供电脑验证。

## 修改内容

1. 确认 UART0 配置为 PA1 RX、PA0 TX、115200 baud。
2. 将 UART0 RX FIFO 阈值设为 1 字节，并启用 RX 与 RX 超时中断。
3. `vision_uart_init()` 同时使能 RX 与 RX 超时中断；已有中断处理函数会将字节写入环形缓冲，再由主循环按换行解析。
4. 未修改 AIM 协议解析。`AIM,0,0,0,255,159,0,0,blob-fallback,NO_LASER` 符合既有格式：7 个数值字段后接目标模式和激光模式；该帧产生 `target_valid=true`、目标坐标 `(255,159)`、`aim_valid=false`。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_uart.c`

## 验证情况

- SysConfig 生成成功，生成代码包含 UART0 RX、RX timeout 中断以及 1 字节 RX FIFO 阈值。
- 使用 TI Arm Clang 编译主程序、生成配置、步进电机和视觉模块源文件，编译通过。
- 未实机验证。发送端必须在文本结尾发送 `\n` 或 `\r\n`，否则解析器不会发布该帧。
- 成功解析后，现有 `main.c` 将从 PA0 回传 `TV,1,-1,-1,255,159\n`，可用 TTL 的 RX 端观察。

## 未处理事项

- 本次仅验证接收和协议解析，不将观测值传入 yaw/pitch 跟踪器。
- TTL 测试需三线连接：TTL TX → PA1、TTL RX ← PA0、GND 共地，115200 8N1。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_packet.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_uart.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Tool: `functions.exec`，命令 `C:\ti\sysconfig_1.21.0\sysconfig_cli.bat ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.exec`，命令 `C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
