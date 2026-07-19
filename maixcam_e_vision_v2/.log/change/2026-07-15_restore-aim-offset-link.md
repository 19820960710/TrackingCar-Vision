# 恢复 AIM dx/dy 输出与偏移连线

## 背景

MSPM0 全双工联调修改在中途被打断，`config.py` 留下不完整的 `MSPM0_READY_PREFIX` 赋值。程序在导入配置时发生 SyntaxError，导致相机主循环、UART AIM 帧发送和屏幕叠加层均未运行。

## 修改

- 修复 `config.py` 的语法错误，保留 A16/UART0_TX 与 A17/UART0_RX 映射配置。
- 未改变 `protocol/ascii_aim.py` 的误差定义：`dx=target_x-laser_x`、`dy=target_y-laser_y`。
- 在 `drivers/display.py` 增加靶心到激光点的绿色偏移连线；只有 target/laser 均有效时绘制。
- 增加显示回归断言，验证连线端点与当前 target/laser 坐标一致。

## 验证

- `python -m py_compile config.py main.py drivers/display.py drivers/uart_device.py`：通过。
- `python -m unittest discover -s tests -v`：67 项通过。
- 协议测试确认新鲜的 target/laser 生成 `AIM,1,dx,dy,...`，过期目标生成 LOST。

## 硬件边界

本机无法替代 MaixCAM 与 MSPM0 实机串口验收。部署后需确认 `/boot/configs` 中 `maix_comm_method=none`，接线为 Maix A16 TX -> MSPM0 RX、A17 RX <- MSPM0 TX、GND 共地，并使用 3.3 V UART 电平。