# MaixCAM UART3 接收烧录说明

本目录是 `empty_routine` 的副本，原始目录未被修改。

## 已集成内容

- SysConfig：MCU UART3，115200 baud，PB2=TX、PB3=RX，RX 和 RX timeout 中断。
- `vision_comm/`：MaixCAM `TV,...` 与 `AIM,...` 文本接收、环形缓冲和观测时效检查。
- `main.c`：1 ms SysTick 时间基、UART3 中断入口和 UART 接收主循环。
- Keil 工程：新增 `VisionComm` 文件组，包含三个通信 `.c` 文件。

## 接线

```text
MaixCAM A19 TX -> 板载 UART4 RX -> MSPM0 PB3
MaixCAM A18 RX <- 板载 UART4 TX <- MSPM0 PB2
MaixCAM GND    -> 板载 UART4 GND
```

仅连接 TX、RX、GND。不要把 UART4 的 5 V 接到 MaixCAM 电源；两端必须共地。

## MaixCAM 配置

在 MaixCAM `config.py` 设置：

```python
ENABLE_UART_OUTPUT = True
UART_OUTPUT_MODE = "target"
UART_PORT = "/dev/ttyS1"
UART_BAUDRATE = 115200
UART_TX_PIN = "A19"
UART_RX_PIN = "A18"
```

`target` 模式输出 `TV,valid,dx,dy,x,y,mode`，最适合当前仅跟踪目标的阶段。

## Keil 烧录与首次验证

1. 打开 `keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx`。
2. Build。工程会在构建前运行 SysConfig 并生成 UART3 配置。
3. 使用已有的 MSPM0 下载器配置下载。
4. 启动 MaixCAM 并让画面中出现目标。

逻辑上，主控收到有效目标帧时 PA14 LED 被置位；目标丢失或超过 100 ms 未收到新帧时被清零。若板上 LED 为低电平点亮，肉眼现象会相反，以 PA14 逻辑电平为准。

## 当前边界

该工程只验证 MaixCAM 到 MSPM0 的通信，不控制云台电机。电机 UART3/UART5、X42S 命令、外环跟踪和机械限位将在后续阶段接入。
