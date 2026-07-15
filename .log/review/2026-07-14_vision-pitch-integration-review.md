# 当前视觉-俯仰跟踪链路审查

日期：2026-07-14
范围：`main.c`、`vision_comm/vision_uart.c`、`pitch_tracker_control.c`、`pitch_motor_control.c`、`empty.syscfg`。

## 已确认正常的部分

- 当前硬件通道配置为：MaixCAM 使用 UART0（PA0 为 TX、PA1 为 RX、115200 baud）；俯仰电机使用 UART1（PB6 为 TX、PB7 为 RX、115200 baud）。PA23 已不在当前工程的 UART 数据通道中。
- `SYSCFG_DL_init()` 会初始化两路 UART；`vision_uart_init()` 额外开启 UART0 接收中断和 NVIC。
- `UART0_IRQHandler()` 会转交给 `vision_uart_on_uart_irq()`，与 MaixCAM 的 UART0 配置一致。
- 使用 TI Arm Clang 对现有全部 C 源文件进行编译检查，结果通过，无编译错误。

## 必须修正的问题（P0）

`main.c` 中已经初始化了 `g_pitch_tracker`，但主循环只依次执行视觉接收、将观测值回传为 `TV,...` 文本、轮询电机；没有任何位置调用 `pitch_tracker_update()`。

结果是：即便 MaixCAM 的数据已被正确解析，俯仰电机也不会收到由目标误差生成的运动命令，当前工程不能实现视觉跟踪。

## 集成时的注意事项（P1）

`relay_latest_maixcam_observation()` 通过 `vision_uart_take_latest_observation()` 取走最新观测值。该接口是消费式读取；后续若在其他地方再次取数据，通常会取不到同一帧。

建议的最小修改是在该函数已获得 `observation`、已计算 `dy` 后，立即调用一次：

```c
(void)pitch_tracker_update(&g_pitch_tracker,
                           observation.target_valid,
                           (int16_t)dy,
                           g_monotonic_ms);
```

随后保留 `TV,...` 回传，便于串口验证。这样每一帧只消费一次，且同一帧同时用于电机控制和日志输出。

## 参数与上电风险

- 当前跟踪器的死区为 8 像素、每次最大 30 脉冲、最短下发周期 40 ms。这是一层低速增量控制，电机自身位置环闭合，因此现阶段不需要再给 MSP 写位置 PID。
- `positive_error_is_cw = true` 只是方向假设；实际装机后需用小目标上/下移动验证，若方向相反，只改该布尔值。
- 调试阶段保持 `PITCH_MOTOR_COMMISSIONING_TEST_ENABLED` 为 0；该测试一旦启用会执行正、反各 1600 脉冲，必须先确认机械行程和限位安全。
- `vision_uart_send_line()` 为阻塞发送。115200 baud 下当前约 20--30 字节诊断行约占数毫秒，短期可用；若后续视觉帧率升高，建议改为非阻塞日志或降低回传频率。

## 建议的下一步

只实施上述一处 `pitch_tracker_update()` 调用，并保持 `TV,...` 回传不变。烧录后先用固定目标测试：目标在画面中心时不动，向上/下偏离死区后仅俯仰轴做小幅、同向修正。
