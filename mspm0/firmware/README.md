# MSPM0G3507 云台固件工程

这个目录保存当前用于烧录的 MSPM0G3507 Keil 工程源文件；入口为 `keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx`。

## 当前串口分配

| 功能 | UART | 引脚 | 波特率 |
| --- | --- | --- | --- |
| Yaw 步进电机 | UART2 | PA23 TX / PA24 RX | 115200 |
| Pitch 步进电机 | UART1 | PB6 TX / PB7 RX | 115200 |
| MaixCAM | UART3 | PB2 TX / PB3 RX | 115200 |

当前 `main.c` 的临时诊断每秒通过 Yaw 封装从 PA23 发送速度查询帧：`01 35 6B`。这只验证 MCU 的发送路径，不会让电机运动。

## Keil 使用方式

工程依赖 TI MSPM0 SDK `2.05.01.00` 的原始 `source/` 目录。该目录约 155 MB、属于厂商 SDK，因此未提交到本仓库。

1. 从同版本 TI SDK 或原始空工程取得 `source/`。
2. 将其放到 `mspm0/firmware/source/`。
3. 用 Keil 打开 `mspm0/firmware/keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx`，Build 后下载。

不要提交 `keil/Objects/`、`.uvoptx`、`.uvguix`、`.axf`、`.hex` 或 `.map`。

## 代码归属

- `main.c`：初始化、主循环、临时串口诊断。
- `gimbal_motor.*`：X42S Emm UART 指令封装。
- `../vision_comm/`：MaixCAM 接收、协议解析和目标观测接口。
- `empty.syscfg`、`ti_msp_dl_config.*`：当前板级 SysConfig 配置和生成结果。
