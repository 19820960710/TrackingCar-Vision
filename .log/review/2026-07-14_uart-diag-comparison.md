# 2026-07-14 串口诊断对比：TrackingCar_Vision vs Stepper_motor_test

## 检查范围

- TrackingCar_Vision 固件 UART 相关源码：`main.c`、`gimbal_motor.c/h`、`vision_uart.c/h`、`vision_config.h`、`ti_msp_dl_config.c/h`、`empty.syscfg`
- Stepper_motor_test（工作正常的参考项目）UART 相关源码：`main.c`、`stepper_motor.c/h`、`zdt_x42s.c/h`、`ti_msp_dl_config.c/h`、`empty.syscfg`
- Keil 启动文件 `startup_mspm0g350x_uvision.s`（确认中断向量表）

## 检查依据

- 参考项目 Stepper_motor_test 的电机串口能够正常收发，作为功能基线对比
- TI MSPM0G3507 DriverLib 手册中的 UART FIFO 行为
- SysConfig v1.21.0 生成的初始化代码

## 发现

### 1. [严重] gimbal_motor 缺少接收函数，电机 UART RX FIFO 必然溢出

`gimbal_motor.c` 实现了发送命令的 `send_bytes()`，但完全没有任何从 UART 读取电机回复的函数。

**影响：**
- `main.c:92` 每 1 秒调用 `gimbal_motor_request_speed()` 通过 UART2(PA23/PA24) 发送 `01 35 6B` 速度查询帧
- 电机收到后回复 4 字节数据帧，但无人读取
- UART2 启用了 FIFO（阈值 1/2 满 = 2 字节触发），但 ISR 未使能 → 数据仅靠硬件 FIFO 暂存
- FIFO 填满后后续字节触发溢出，UART 状态寄存器标记 OE（Overrun Error）
- 溢出后 UART 可能丢失后续所有数据，直到复位

**证据：**
- `gimbal_motor.h` 无 poll/read 函数声明（共 4 个函数：init/set_enabled/stop/move_relative/request_speed）
- `gimbal_motor.c` 无任何从 UART RX 读取的代码
- 对比 `stepper_motor_test/zdt_x42s.c:71` 的 `ZdtX42s_pollResponse()` 正确读取并解析回复
- `ti_msp_dl_config.c:186-193` FIFO 已启用，RX 阈值为 `DL_UART_RX_FIFO_LEVEL_1_2_FULL`

**处理：** 待修复。需在 `gimbal_motor.c/h` 添加 `gimbal_motor_poll_response()`。

### 2. [中] SYSPLL 失锁风险 → maxicam UART3 波特率错误

maxicam UART3 时钟源为 BUSCLK（80 MHz），波特率分频系数 IBRD=43/FBRD=26 基于 80 MHz 计算。若 SYSPLL 锁定失败，系统运行在 SYSOSC 32 MHz，则 UART3 实际波特率约为 46 kbps，与 MaixCAM 的 115200 不匹配。

**影响：** 如果 PLL 失锁，maxicam 收到的全是帧错误数据，完全无法通信。

**证据：**
- `ti_msp_dl_config.c:247-256` maxicam UART 配置 BUSCLK 时钟源 + 80 MHz 分频系数
- `ti_msp_dl_config.c:128-157` SYSPLL 配置（QDIV=5、PDIV=1）
- 如果 PLL 未锁定，实际 BUSCLK=32 MHz → 实际波特率 = 32000000/(16×43.406) ≈ 46076

**处理：** 需要实测验证。逻辑分析仪测 TX 引脚位宽是否 8.68 μs。

### 3. [低] 电机 UART 时钟源与参考项目不同

TrackingCar_Vision 电机 UART 使用 MFCLK（4 MHz），Stepper_motor_test 使用 BUSCLK（32 MHz）。两者计算的波特率都在 115200±0.1% 内，但 4 MHz 的定时分辨率（每个位周期 4 个采样点）比 32 MHz（每周期 32 个采样点）粗。

**影响：** 两者波特率误差都在规格内（< 2%），理论不会因此导致通信失败。但如果 MFCLK 因时钟树配置偏差偏离 4 MHz，波特率会成比例偏移。

**证据：**
- TrackingCar_Vision `ti_msp_dl_config.c:161-164` 使用 `DL_UART_MAIN_CLOCK_MFCLK`
- Stepper_motor_test `ti_msp_dl_config.c:140-143` 使用 `DL_UART_MAIN_CLOCK_BUSCLK`
- TrackingCar_Vision `empty.syscfg:45-46` 显式设置 `uartClkSrc = "MFCLK"`

**处理：** 暂不处理。如果排查发现电机 UART 也不通，优先验证 MFCLK 频率。

### 4. [低] maxicam UART TX 从未被调用

`vision_uart.c:218-229` 实现 `vision_uart_send_line()`，使用 `DL_UART_Main_transmitDataBlocking` 发送文本行。但 `main.c` 中未调用该函数，UART3 的 TX 方向未被固件测试。

**影响：** 如果串口问题发生在 TX 方向，此路径未经验证。

**证据：**
- `main.c` 搜索 `vision_uart_send` 无结果
- `vision_uart.h:54-55` 声明了该函数

**处理：** 暂不处理。明确需要时再启用。

### 5. [低] UART 中断使能顺序存在短暂窗口

`SYSCFG_DL_maxicam_init()` 使能 UART3 外设级中断，但 NVIC 中断在稍后的 `vision_uart_init()` 中使能。两者之间若收到数据，中断请求会被 NVIC 屏蔽，但 `NVIC_ClearPendingIRQ()` 会清除该挂起请求。

**影响：** 窗口非常短（< 若干指令周期），UART RX 通常需要有数据到达才会触发，首次使能前一般不会有数据。理论风险极低。

**证据：**
- `ti_msp_dl_config.c:260-262` 使能 UART 中断
- `vision_uart.c:157` `NVIC_ClearPendingIRQ` + `NVIC_EnableIRQ`
- `main.c:181-183` `SYSCFG_DL_init()` → `vision_uart_init()`

**处理：** 暂不处理。风险可接受。

## 已采纳

本次为纯检查，未修改代码。无采纳项。

## 未采纳

无。

## 验证情况

未进行硬件验证。以上发现基于代码分析，UART 实际行为需要在硬件上通过逻辑分析仪或串口工具确认。

## 依据与工具

- Skill: `/home/firefly/.claude/skills/project-logbook/SKILL.md`
- Source: 以下文件的直接读取
  - `TrackingCar_Vision/mspm0/firmware/main.c`
  - `TrackingCar_Vision/mspm0/firmware/gimbal_motor.c`
  - `TrackingCar_Vision/mspm0/firmware/gimbal_motor.h`
  - `TrackingCar_Vision/mspm0/firmware/ti_msp_dl_config.c`
  - `TrackingCar_Vision/mspm0/firmware/ti_msp_dl_config.h`
  - `TrackingCar_Vision/mspm0/firmware/empty.syscfg`
  - `TrackingCar_Vision/mspm0/vision_comm/vision_uart.c`
  - `TrackingCar_Vision/mspm0/vision_comm/vision_uart.h`
  - `TrackingCar_Vision/mspm0/vision_comm/vision_config.h`
  - `TrackingCar_Vision/mspm0/vision_comm/vision_packet.c`
  - `TrackingCar_Vision/mspm0/firmware/keil/startup_mspm0g350x_uvision.s`
  - `Stepper_motor_test/main.c`
  - `Stepper_motor_test/stepper_motor.c/h`
  - `Stepper_motor_test/zdt_x42s.c/h`
  - `Stepper_motor_test/ti_msp_dl_config.c/h`
  - `Stepper_motor_test/empty.syscfg`
- Tool: `Bash`，command `find ... && sort`，cwd `/home/firefly/projects/`
- Tool: `Read`，多个文件的读取
