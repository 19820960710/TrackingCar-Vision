# MaixCAM UART电脑仿真

## 用途

不连接MaixCAM，由电脑通过USB-TTL按接近UART物理上限的频率连续发送AIM帧，同时读取主控回传的视觉和电机诊断数据。

默认参数为115200 bit/s、250 Hz，单帧约4 ms，与当前AIM文本在115200 8N1下的线速时间接近。

## 接线

- USB-TTL TXD -> MSPM0 PA1（UART0 RX）
- USB-TTL RXD -> MSPM0 PA0（UART0 TX）
- USB-TTL GND -> MSPM0 GND
- pitch电机TXD -> PB7（用于接收电机应答）
- yaw电机TXD -> PA24（用于接收电机应答）
- 所有设备共地

## 运行

```powershell
python -m pip install pyserial
python .\tools\maixcam_uart_simulator.py --port COM12 --log .\uart_simulation.log
```

将`COM12`替换为实际USB-TTL串口。按`Ctrl+C`停止。

若实测摄像头不是250 Hz，可指定帧率：

```powershell
python .\tools\maixcam_uart_simulator.py --port COM12 --fps 200
```

## 仿真序列

脚本循环发送以下场景，每个场景内仍按设定帧率连续发送：

1. 中心
2. yaw正误差、回中
3. yaw负误差、回中
4. pitch正误差、回中
5. pitch负误差、回中
6. 双轴正误差、回中
7. 双轴负误差、回中

误差为40像素，当前控制参数对应单次320脉冲。正负动作在一个周期内抵消。

## 主控输出

视觉行：

```text
TV,valid,dx,dy,x,y,CS,pitch_submit,yaw_submit
```

`pitch_submit`和`yaw_submit`为1表示该帧成功把运动命令提交给对应电机发送状态机。

电机状态行：

```text
MS,axis,Q,J,F,T,A,R,P,E
```

- `axis`：`Y`为yaw，`P`为pitch
- `Q`：成功排队的运动命令数
- `J`：被拒绝的运动命令数
- `F`：发送完成的协议帧数，包含使能和自检帧
- `T`：发送超时数
- `A`：电机返回命令已接受`02`的次数
- `R`：电机返回到位`9F`的次数
- `P`：电机返回保护错误`E2/EE`的次数
- `E`：返回帧协议错误次数
