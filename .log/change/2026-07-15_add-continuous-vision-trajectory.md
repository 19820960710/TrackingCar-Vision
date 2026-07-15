# 2026-07-15 增加连续视觉误差轨迹

## 修改目标

- 在不连接 MaixCAM 时，以真实串口帧率向主控发送连续变化的目标误差，覆盖双轴同时、正反方向和过中心运动。
- 保留已有固定场景测试，避免影响此前双轴故障验收流程。

## 修改内容

1. 为电脑端 UART 仿真器增加 `trajectory` 模式。
   - 默认轨迹为以画面中心为原点的椭圆。
   - `dx=120×sin(2πt/8)`，`dy=70×cos(2πt/8)`。
   - 开始和结束各使用 1 秒 smoothstep 包络，使误差从零平滑进入并回到中心。
2. 增加轨迹参数。
   - `--trajectory-duration`
   - `--trajectory-period`
   - `--yaw-amplitude`
   - `--pitch-amplitude`
   - `--trajectory-ramp`
3. 增加 `--generate-only`，无需打开串口即可输出连续误差 CSV。
4. 连接主控运行时，在会话目录额外保存 `trajectory.csv`，并继续保存 `serial.log` 与 `summary.json`。
5. 新增连续轨迹使用说明和一份默认 100 Hz 测试向量。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\maixcam_uart_simulator.py`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\README_continuous_trajectory.md`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\test_vectors\continuous_ellipse_100Hz.csv`

## 验证情况

- Python `py_compile` 通过。
- 离线生成 1600 个误差点，对应 16 秒、100 Hz、两个椭圆周期。
- 数组范围为 `dx=-120..120`、`dy=-70..70`。
- 首帧和末帧均为 `(0,0)`。
- 相邻帧最大变化为 `|Δdx|=2`、`|Δdy|=1` 像素，没有固定场景切换时的误差突跳。
- 本次未通过串口驱动实机；运行前仍需等待主控自检完成并返回 `VISION_READY`。

## 未处理事项

- 当前轨迹用于模拟画面内连续目标运动，不包含镜头畸变、识别噪声、丢帧或时间戳抖动；这些可在基础闭环稳定后单独增加。
- 默认 100 Hz 沿用当前验收频率。若实测 MaixCAM 帧率不同，需要通过 `--fps` 改为实测值。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\maixcam_uart_simulator.py`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\change\2026-07-15_fix-dual-axis-stack-and-binding.md`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.shell_command`，command `python -m py_compile tools\maixcam_uart_simulator.py`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.shell_command`，command `python tools\maixcam_uart_simulator.py --mode trajectory --fps 100 --generate-only tools\test_vectors\continuous_ellipse_100Hz.csv`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
