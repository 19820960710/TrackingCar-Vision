# ZDT X42S 双 UART FreeRTOS 说明

## 当前硬件映射

两个驱动器不共享总线，因此设备地址都保持 `0x01`：

| 轴 | MCU 外设 | TX | RX | 波特率 | 地址 |
|---|---|---|---|---|---|
| yaw | UART1 | PB6 | PB7 | 115200 8N1 | `0x01` |
| pitch | UART2 | PA23 | PA24 | 115200 8N1 | `0x01` |

TX/RX 名称均以 MCU 为视角：MCU TX 接驱动器 RX，MCU RX 接驱动器 TX，两路必须与 MCU 共地。

## 分层

1. `Component/UART/stepper_uart.*`：分别拥有 UART2/UART1 的发送互斥锁、RX 中断和字节队列。
2. `Component/zdt_x42s/zdt_x42s.*`：纯协议层，组装使能帧、位置帧并解析响应。
3. `Component/zdt_x42s/stepper_motor.*`：电机语义包装。
4. `Component/service/stepper_service.*`：把 yaw/pitch 轴映射到各自 UART，提供命令队列和独立状态快照。
5. `Component/task/app_tasks.*`：提供比赛逻辑可调用的轴控制 API。

旧的 `uart0.*` 和 UART0 角色开关已经移除，步进二进制串口不承载 `printf` 调试文本。

## 应用层调用

```c
app_stepper_move_t move = {
    .direction = APP_STEPPER_DIRECTION_CW,
    .speed_rpm = 30,
    .acceleration = 10,
    .pulse_count = 800,
    .motion_mode = ZDT_X42S_MOTION_REALTIME_RELATIVE,
    .sync_flag = 0,
};

app_tasks_set_stepper_axis_enabled(APP_STEPPER_AXIS_YAW, true);
app_tasks_move_stepper_axis(APP_STEPPER_AXIS_YAW, &move);

app_tasks_set_stepper_axis_enabled(APP_STEPPER_AXIS_PITCH, true);
app_tasks_move_stepper_axis(APP_STEPPER_AXIS_PITCH, &move);
```

旧的 `app_tasks_set_stepper_enabled()`、`app_tasks_move_stepper()` 和
`app_tasks_get_stepper_state()` 仍保留，默认操作 yaw 轴。

## 实机验证限制

- 当前构建验证不能替代实机 TX/RX、方向、限位和驱动器响应测试。
- `3200 pulse/rev` 是当前细分假设，驱动器细分改变后需同步修改运动参数。
- 两个轴由同一服务任务先后发帧，启动相差若干毫秒，并非驱动器硬同步。
