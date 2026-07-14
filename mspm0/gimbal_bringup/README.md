# MSPM0 云台 UART 联调快照

本目录是 `gimbal-uart-diagnostic` 分支的当前 X42S 驱动快照，供后续将云台接入完整 Keil 工程时使用。

当前硬件映射：

- Yaw：UART2，PA23 TX / PA24 RX，115200 bit/s，MFCLK。
- Pitch：UART1，PB6 TX / PB7 RX，115200 bit/s，MFCLK。

当前板级诊断版本暂时关闭电机自检；诊断帧为 `01 35 6B`。最新烧录工程中，该帧被直接从 UART1/PB6 发送以隔离 UART 引脚问题。该工程本身不在本仓库中，因此本目录不包含 Keil 文件、TI SDK 或 SysConfig 生成文件。

`gimbal_motor.c` 的发送路径为：等待 TX FIFO 有空间后调用 `DL_UART_Main_transmitData()`。位置命令使用 EMM `FD` 模式，固定为相对当前实际位置（mode 2）。
