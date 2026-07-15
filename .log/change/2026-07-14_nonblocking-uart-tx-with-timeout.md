# 2026-07-14 全部运行期 UART 发送改为非阻塞超时状态机

## 修改目标

- 消除电机 UART 和 MaixCAM 回传 UART 中无限等待 TX FIFO 的路径，避免单路串口异常导致主循环、LED、巡线和其他任务全部停止。
- 让 PB6/PB7 pitch 与 PA23/PA24 yaw 各自独立推进发送状态。

## 修改内容

1. 重构 X42S 电机发送。
   - 删除 `while (DL_UART_Main_isTXFIFOFull())` 死等。
   - 每个 `ZdtX42s` 实例保存自己的待发帧、当前索引、开始时间和超时次数。
   - `ZdtX42s_serviceTx()` 每次只写入当前 FIFO 可接收的字节；FIFO 满时立即返回。
   - 待发帧超过 20 ms 仍未提交完时丢弃，不阻塞其他任务。
   - 当前帧尚未发送完时拒绝覆盖；上层收到 `false` 后可在后续调度周期重试。
2. 重构电机封装接口。
   - `enable` 和 `move` 返回是否成功提交到该电机的独立发送上下文。
   - 删除启动阶段 `delay_cycles(100000U)`。
   - 自检状态机仅在运动帧成功提交后才切换状态。
3. 重构视觉回传发送。
   - 删除 `DL_UART_Main_transmitDataBlocking()`。
   - `vision_uart_send_line()` 只复制一条待发文本并立即返回。
   - `vision_uart_service_tx()` 分片写入 FIFO，20 ms 超时后丢弃该行。
   - 增加 TX busy 丢弃和超时统计字段。
4. 主循环调度。
   - 每圈分别服务 pitch、yaw 和 vision 三个 TX 状态机。
   - 任一路异常不会阻止 PB22 心跳及其他状态机继续更新。

## 涉及文件

- `zdt_x42s.c`
- `zdt_x42s.h`
- `stepper_motor.c`
- `stepper_motor.h`
- `pitch_motor_control.c`
- `pitch_motor_control.h`
- `vision_comm/vision_uart.c`
- `vision_comm/vision_uart.h`
- `main.c`

## 验证情况

- 使用 TI ARM Clang 编译全部应用源文件，退出码为 0。
- 扫描 Keil 当前参与构建的运行期通信源文件，未再发现 `transmitDataBlocking`、等待 TX FIFO 的无限循环或运行期 `delay_cycles`。
- 尚未实机验证。烧录后即使 TV 输出再次停止，PB22 也应继续闪烁；若某一路 UART 卡住，该路帧会在 20 ms 后丢弃。

## 未处理事项

- 当前采用主循环协作式调度，不是 RTOS 线程；后续巡线、按键和其他控制任务也必须使用短执行时间的 `update/service` 接口。
- 暂未通过 UART TX 中断发送；当前轮询服务每次执行有明确上界，满足现阶段非阻塞要求。
- pitch 视觉跟踪不动作的根因仍需实机复测；本次先消除会冻结整个系统的发送结构问题。
- 工程根目录保留的旧版 `gimbal_motor.c` 仍含阻塞发送，但 Keil 工程未引用该文件，当前固件使用的是 `zdt_x42s.c`、`stepper_motor.c` 和 `pitch_motor_control.c`。后续不应重新把旧文件加入构建。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\zdt_x42s.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_uart.c`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.shell_command`，command `tiarmclang -c ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
