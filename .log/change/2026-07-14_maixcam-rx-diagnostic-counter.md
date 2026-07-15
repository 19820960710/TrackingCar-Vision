# 2026-07-14 MaixCAM 接收计数诊断回传

## 修改目标

- 在现场已确认 MaixCAM 正在发送 AIM 帧的前提下，通过 PA0 周期回传接收统计，区分 PA0 输出链路、PA1 未收到字节和协议解析失败。

## 修改内容

1. 在 `main.c` 增加每 1000 ms 执行的 `vision_uart_diagnostic_update()`。
2. 通过既有 UART0 PA0 TX 回传 `VU,packet_count,parse_error_count,overrun_count`。
3. 保留成功观测的即时 `TV,...` 回传，不改变 AIM/TV 解析与电机逻辑。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 使用 TI Arm Clang 编译主程序、生成配置、步进电机和视觉模块源文件，编译通过。
- 未实机验证。烧录后，TTL 监听 PA0 应每秒至少收到一行 `VU,0,0,0`；若 MaixCAM 的有效 AIM 帧到达，应额外收到 `TV,1,-1,-1,255,159` 类回传，且第一计数递增。

## 未处理事项

- 诊断行临时保留，后续完成通信验证后应关闭或移除，避免正式追踪阶段占用 UART 带宽。
- 本次不以本地 MaixCAM 源码推断现场烧录程序；现场截图确认 MaixCAM 实际输出为 AIM 行。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_uart.c`
- Source: 现场 MaixCAM UART 截图 `C:\Users\Aupassen\AppData\Local\Temp\codex-clipboard-b5e0364b-0f55-4d4f-a191-ffb09b2a90d9.png`
- Tool: `functions.exec`，命令 `C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
