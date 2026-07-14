# 2026-07-14 Yaw UART2 周期查询

## 修改目标

- 让步进电机封装的周期查询实际从 Yaw 所在的 UART2/PA23 发出，便于与已验证可用的封装调用方式对照。

## 修改内容

1. 将 `main.c` 的周期诊断从直接写 `stepMotor2_INST` 改为调用 `gimbal_motor_request_speed(&g_yaw_motor)`。
   - Yaw 配置持有 `stepMotor1_INST`，即 UART2/PA23。
   - 每个周期仍发送 EMM V5 速度查询帧 `01 35 6B`，周期仍为 1000 ms。
2. 将编译开关重命名为 `GIMBAL_UART_DIAGNOSTIC_ENABLED`。
   - 该路径不再是“裸 UART 发送”，名称与实际行为一致。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\change\2026-07-14_yaw-uart2-periodic-query.md`

## 验证情况

- 静态检查：调用在 `gimbal_motor_init(&g_yaw_motor, &g_yaw_motor_config)` 之后的主循环执行；`g_yaw_motor` 的 UART 实例为 UART2/PA23。
- 待实机验证：USB-TTL 的 RXD 接 PA23、GND 接开发板 GND，串口助手设为 115200、8N1、HEX，应每秒收到一次 `01 35 6B`。

## 未处理事项

- 未改动 SysConfig、UART 时钟、波特率或引脚复用；本次只替换发送调用路径。
- 未验证电机返回帧和实际运动；本次仅验证 MCU 的 TX。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\review\2026-07-14_gimbal-uart-send-path-audit.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Tool: `functions.exec`，`apply_patch`，cwd `C:\Users\Aupassen\Desktop\视觉`
