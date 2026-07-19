# 二进制协议 V1 与 MSPM0 安全策略

二进制协议尚未启用。V2 首版保持 `ASCII AIM,...`；只有 MSPM0 固件完成本文件的解析和验收后，才可把 `UART_PROTOCOL_MODE` 改为 `binary_v1`。

## 帧格式（小端，固定 22 字节）

| 字段 | 字节 | 说明 |
|---|---:|---|
| Header | 2 | `0xA5 0x5A` |
| Version | 1 | `0x01` |
| Sequence | 2 | 无符号递增序号，允许 16 位回绕 |
| Status | 1 | 状态位 |
| age_ms | 2 | 距最后真实目标量测的毫秒数 |
| target_x, target_y | 4 | `int16` 像素坐标 |
| laser_x, laser_y | 4 | `int16` 像素坐标 |
| dx, dy | 4 | `int16`，仅目标与激光都有效时非零 |
| CRC16 | 2 | CRC-16/IBM，覆盖 Header 到 dy |

状态位：`bit0 TARGET_VALID`、`bit1 LASER_VALID`、`bit2 TARGET_PREDICTED`、`bit3 TARGET_LOST`、`bit4 TARGET_STALE`。

## MSPM0 决策表

| 情形 | 动作 | 是否更新电机误差 | 是否刷新 100 ms 看门狗 |
|---|---|---|---|
| 正常帧、目标和激光有效 | `FOLLOW` | 是 | 是 |
| 预测帧 | 默认 `HOLD_PREDICTED` | 否 | 是 |
| 目标有效、激光无效 | `HOLD_NO_LASER` | 否 | 是 |
| `TARGET_LOST` / `TARGET_STALE` / `age_ms>100` | `STOP_LOST` | 否，停止 | 否 |
| 旧序号 | `DROP_OLD` | 否 | 否 |
| CRC 错、帧头错、版本错、长度错 | `DROP_*` | 否 | 否 |
| 超过 100 ms 未收到可接受帧 | `STOP_WATCHDOG` | 否，停止 | — |

`protocol/mspm0_safety_reference.py` 是测试用 Python 参考实现，不是 MCU 固件。MSPM0 必须使用相同的序号比较、CRC 和安全策略后，才可启用二进制发送。
