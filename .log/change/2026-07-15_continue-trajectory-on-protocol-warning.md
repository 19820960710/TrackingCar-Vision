# 2026-07-15 协议解析错误不再立即中止轨迹

## 修改目标

- 避免电脑仿真器把主控本地返回帧解析错误 `E` 当成电机保护 `P`，导致安全测试在首次诊断输出时提前结束。

## 修改内容

1. `P>0` 仍立即停止轨迹。
2. `E` 增长时输出 WARN 并继续发送，同时在 summary 中记录 `protocol_error_observed`。
3. 完成但出现 E 时使用 `complete_with_protocol_errors` 状态，区别于无错误完成。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\maixcam_uart_simulator.py`

## 验证情况

- Python `py_compile` 通过。
- 第一次测试因 Pitch `E=3` 在约 0.5 秒结束。
- 修改后相同 100 Hz、16 秒轨迹完整运行，最终两轴均为 `Q=80,F=81,A=81,J=0,T=0,P=0,E=0`。
- 用户确认两轴在 16 秒内同时运动；随后长周期测试也持续运行，停止脚本后 COM11 已释放。

## 未处理事项

- 第一次 Pitch `E=3` 的原始四字节内容未保存，后续相同条件未复现，暂不归因。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\test_logs\20260715-live-trajectory\20260715-151834_100Hz_trajectory\summary.json`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\test_logs\20260715-live-trajectory-retry\20260715-152908_100Hz_trajectory\summary.json`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.shell_command`，command `python -m py_compile tools\maixcam_uart_simulator.py`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
