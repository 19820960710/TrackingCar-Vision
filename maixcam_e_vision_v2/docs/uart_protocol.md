# UART0 固定接口

首版默认单向发送：A16/UART0_TX 接 MSPM0 UART0_RX，GND 共地；不连接 +5 V/VBUS。A17/UART0_RX 仅在双向命令正式启用后才接线并映射。

固定参数：`/dev/ttyS0`、115200、8N1、无流控。MaixCAM `/boot/configs` 必须设置 `maix_comm_method=none` 并重启。A16 不能在复位时被外部拉低；UART0 开机日志不是视觉数据，MSPM0 必须丢弃。

首版发送 ASCII `AIM,...` 帧以兼容当前 MSPM0。目标无效、lost-hold 或数据年龄超过 100 ms 时，视觉端发送 LOST；MSPM0 必须额外使用 100 ms 接收看门狗停止继续跟随。
