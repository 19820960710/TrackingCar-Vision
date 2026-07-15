# 2026-07-15 延长 PD 基线回正观察窗

## 修改目标

- 让纯 P 基线有足够时间进入 ±4 像素稳定带，完成四个方向测试。

## 修改内容

1. 将自动调参回正超时从 4 秒延长到 8 秒。
2. 保持 Yaw/Pitch `Kp=0.2`、`Kd=0` 和所有控制参数不变。

## 涉及文件

- `main.c`
- `.log/change/2026-07-15_extend-pd-baseline-response-window.md`

## 验证情况

- 4 秒基线中 Yaw 正向误差从 60 像素平滑降至 8 像素，无过零、振荡或目标丢失；原状态机因未进入 ±4 像素稳定带而中止。
- 延长后的四方向测试仍需实机采集。

## 未处理事项

- 尚未修改控制参数；本次只修正实验观察窗口。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\pd_tuning_logs\20260715-183302_kp020_kd000_baseline_retry\summary.json`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\pd_tuning_logs\20260715-183302_kp020_kd000_baseline_retry\pd_samples.csv`
- Tool: `functions.shell_command`，CSV 时间序列检查，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
