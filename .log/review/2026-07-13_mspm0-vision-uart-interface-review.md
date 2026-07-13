# 2026-07-13 MSPM0 视觉串口接口审查

## 检查范围

- `mspm0/vision_comm/vision_packet.c/.h`
- `mspm0/vision_comm/vision_uart.c/.h`
- `mspm0/vision_comm/README.md`
- `docs/uart_protocol.md`
- MaixCAM 当前 `AIM` 与 `TV` 文本输出语义

## 检查依据

- 当前阶段目标是让云台摄像头跟随目标，即控制误差应为“目标位置减画面期望位置”，不能依赖激光点有效。
- 板上计划使用：UART3 接 Yaw X42S、UART5 接 Pitch X42S、UART4 接 MaixCAM；对应 MCU 外设分别为 UART2、UART1、UART3。
- 按 `c-style` 检查模块边界、状态所有权、硬件依赖、接口副作用和失效行为。

## 发现

1. 当前公开数据结构以激光精瞄语义为主，不能直接作为摄像头跟踪控制输入。
   - 影响: `aim_valid` 只有目标和激光点同时有效时才为真；第一阶段没有可靠激光点时，若应用按示例读取 `aim_dx/aim_dy`，云台不会跟踪目标。
   - 证据: `mspm0/vision_comm/vision_packet.h` 的 `aim_valid/aim_dx/aim_dy`；`maixcam/drivers/uart_output.py` 中 `aim_output_line()` 的有效条件和误差定义；`mspm0/vision_comm/README.md:85-87` 的调用示例。
   - 处理: 下一步采纳。保留 AIM 解析能力，但增加与协议解耦的目标观测结构；跟踪控制只读取 `target_x/target_y`、画面尺寸、有效标志和接收序号，并由主控计算相对期望像素点的误差。

2. UART 硬件实例被写死为 `UART_0_INST`，与计划接线及可复用接口目标冲突。
   - 影响: 现有模块不能直接绑定板上 UART4 对应的 MCU UART3；复制修改宏会使同一驱动难以复用和审查。
   - 证据: `mspm0/vision_comm/vision_uart.c:51-65,159-162`；`mspm0/vision_comm/README.md:22-40`。
   - 处理: 下一步采纳。把 UART 寄存器实例、中断号或字节收发依赖通过上下文/HAL 注入；协议解析器保持完全不依赖 `ti_msp_dl_config.h`。

3. 接口没有数据新鲜度和通信超时语义。
   - 影响: `updated` 在调用方主动清除前一直为真；如果 MaixCAM 断线，应用可能持续使用旧坐标重复修正云台。
   - 证据: `mspm0/vision_comm/vision_uart.c:109-111,130-149`；数据结构中没有接收时间或序号。
   - 处理: 下一步采纳。每个合法包生成递增序号并记录接收时刻；提供一次性消费或按序号判断的新包接口；超过设定时间后观测失效并保持最后电机位置。

4. 单行缓存会在主循环未及时处理时丢弃整帧。
   - 影响: 一旦 `g_line_ready` 置位，后续所有字节在 `vision_uart_process()` 前都被丢弃；视觉或显示任务阻塞超过一个发送周期时会产生连续丢包。
   - 证据: `mspm0/vision_comm/vision_uart.c:23-25`。
   - 处理: 下一步采纳。改为小型字节环形缓冲区，ISR 只入队，主循环负责按换行符组帧；缓冲区满时记录明确的溢出计数并重新同步。

5. 数字解析缺少严格分隔符和溢出检查，坐标异常被静默钳位。
   - 影响: 缺少逗号的部分畸形帧可能被接受；超长数字在 `int32_t` 乘法中可能溢出；损坏坐标被钳位到边界后可能产生最大幅度错误控制。
   - 证据: `mspm0/vision_comm/vision_packet.c:11-45,47-66,78-94`。
   - 处理: 下一步采纳。要求每个字段前存在逗号，累加前检查范围，控制字段越界时拒绝整帧，不以钳位代替协议校验。

6. `has_target/has_laser` 通过坐标是否非零推断，会误判合法原点坐标。
   - 影响: 位于 `(0,0)` 的目标或激光点会被标记为不存在；状态语义依赖坐标哨兵值，不利于后续更换视觉算法。
   - 证据: `mspm0/vision_comm/vision_packet.c:92-93`。
   - 处理: 下一步采纳。解析尾部状态字段或由明确的协议有效位生成存在标志，不再使用坐标值推断状态。

7. 临界区无条件重新开中断。
   - 影响: 如果调用前系统已经处于关中断临界区，`vision_uart_process/get/clear` 会意外开启全局中断，破坏调用方的临界区。
   - 证据: `mspm0/vision_comm/vision_uart.c:80-94,123-126,138-141,147-149`。
   - 处理: 下一步采纳。优先调整状态所有权以减少临界区；确需保护时保存并恢复 PRIMASK，而不是无条件 `__enable_irq()`。

8. README 接线与当前主控板原理图、既定资源分配不一致。
   - 影响: 文档要求 UART0/PA10/PA11，而当前板上准备从 UART4 接口使用 MCU UART3/PB2/PB3；照文档接线会导致 SysConfig 和实物连接对不上。
   - 证据: `mspm0/vision_comm/README.md:10-40`；本地 `SCH_Schematic1_2026-06-13.pdf` 的串口区。
   - 处理: 下一步采纳。文档同时写清“板上丝印接口”和“MCU UART 外设/引脚”，默认配置改为板上 UART4 → MCU UART3/PB2/PB3。

## 已采纳

- 本次为纯审查，没有修改接口源码；以上问题已作为下一实现步骤的输入。
- 已保留提交中“协议解析与 UART 接收分文件”的方向，后续在此边界上继续拆分协议、传输和统一观测结构。

## 未采纳

- 暂不把文本协议整体替换为二进制 CRC 协议。当前先兼容 MaixCAM 已有输出，降低首次联调同时修改两端的风险；闭环跑通后再作为独立步骤升级。
- 暂不修改 MaixCAM 目标检测算法。本次范围仅为 MSPM0 通信接口。

## 验证情况

- 已将 `TrackingCar-Vision` 从 `b39d1c5` 快进到 `1e2e23d`，原有未提交文件未被覆盖。
- `git diff --check HEAD` 通过，仅报告现有工作区文件未来可能发生 LF/CRLF 转换。
- 当前环境未发现 `gcc` 或 `clang`，未在宿主机重新编译 C 模块；提交日志中记录的 Keil 编译结果未在本次环境复核。
- UART 引脚、IRQ、接收超时和真机数据率均需要实物验证。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\vision_comm\vision_packet.c`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\vision_comm\vision_uart.c`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\vision_comm\README.md`
- Source: `C:\Users\Aupassen\Documents\xwechat_files\wxid_26kfkdytwptx22_a56d\msg\file\2026-07\SCH_Schematic1_2026-06-13.pdf`
- Tool: `functions.shell_command`，command `git fetch --no-tags origin main`、`git merge --ff-only origin/main`、`git diff --check HEAD`，cwd `C:\Users\Aupassen\Desktop\视觉`
