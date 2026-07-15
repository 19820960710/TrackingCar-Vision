# 连续视觉误差轨迹仿真

`maixcam_uart_simulator.py` 的 `trajectory` 模式按指定帧率逐帧生成并发送连续的 `AIM` 数据，用来模拟目标在摄像头画面内平滑移动。

默认轨迹为椭圆：

```text
dx = 120 × envelope × sin(2πt / 8)
dy =  70 × envelope × cos(2πt / 8)
```

`envelope` 在轨迹开始和结束的 1 秒内使用 smoothstep 从 0 平滑变化，避免目标位置突然跳变。默认 16 秒、100 Hz，共生成 1600 个误差点，包含两个完整椭圆周期。

## 仅生成误差数组

```powershell
python .\tools\maixcam_uart_simulator.py `
  --mode trajectory `
  --fps 100 `
  --generate-only .\tools\test_vectors\continuous_ellipse_100Hz.csv
```

CSV 字段为：

```text
index,time_s,dx,dy,target_x,target_y
```

## 连接主控并运行

```powershell
python .\tools\maixcam_uart_simulator.py `
  --port COM11 `
  --baud 115200 `
  --mode trajectory `
  --fps 100 `
  --trajectory-duration 16 `
  --trajectory-period 8 `
  --yaw-amplitude 120 `
  --pitch-amplitude 70 `
  --cycles 3
```

主控完成开机自检并返回 `VISION_READY` 后，脚本才开始发送轨迹。每次运行会在测试会话目录保存：

- `trajectory.csv`：实际发送使用的连续误差数组。
- `serial.log`：主控原始回传。
- `summary.json`：TV、MS 与两轴计数统计。

如果后续测得 MaixCAM 的真实发送频率不是 100 Hz，只需把 `--fps` 改成实测值；轨迹数组长度和串口发送周期会同步调整。
