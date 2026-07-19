# MaixCAM E 题视觉模块 V2

这是独立开发的 V2 视觉模块工程。旧版 `C:\Users\Lenovo\TrackingCar-Vision` 完全冻结，只用于对比、查阅和回退；本工程不会覆盖旧版代码、配置或安装包。

## 第 1 步固定接口

第 1 步的硬件接口、数据字段、安全规则和测试场景已经冻结，详细内容见：[接口契约](docs/interface_contract.md)。

- 默认只发送：MaixCAM A16/UART0_TX → MSPM0 UART0_RX，且两端 GND 共地。
- A17/UART0_RX 默认不映射；只有明确启用双向命令协议时才接线并设置 `UART_ENABLE_RX=True`。
- UART0 固定为 `/dev/ttyS0`、115200、8N1、无流控。
- MaixCAM `/boot/configs` 必须设置 `maix_comm_method=none` 后重启；否则 V2 拒绝打开 UART0。
- 首版保持 `AIM,...` 文本协议兼容现有 MSPM0 接收端。
- 目标无效、`lost_hold=True` 或 `age_ms>100` 时，V2 发送 LOST 帧；MSPM0 仍必须实现 100 ms 接收看门狗并停止继续跟随。

当前版本只完成模块骨架、接口契约和纯逻辑测试；尚未实现靶框识别、YOLO 重捕、405 nm 蓝紫激光识别、单应性和 MaixCAM 实机运行。
