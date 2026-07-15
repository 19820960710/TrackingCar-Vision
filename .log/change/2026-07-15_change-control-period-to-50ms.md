# 2026-07-15 控制周期改为 50 ms 并等效换算 Kp

## 修改目标

- 消除 200 ms、5 Hz 相对位置更新造成的可见顿挫，同时维持上一组已验证参数的近似单位时间控制强度。

## 修改内容

1. 双轴控制周期从 200 ms 改为 50 ms，控制更新频率从 5 Hz 提高到 20 Hz。
2. 按周期比例将 `Kp` 从 0.50 换算为 `0.50 × 50 / 200 = 0.125`。
3. `Kd` 保持 0，方向、死区、速度、扰动和安全条件不变。
4. 撤回尚未构建和执行的 `Kp=0.70, 200 ms` 试验。

## 涉及文件

- `main.c`
- `.log/change/2026-07-15_pd-tuning-kp-070.md`
- `.log/change/2026-07-15_change-control-period-to-50ms.md`

## 验证情况

- 原 `Kp=0.50, 200 ms` 稳定时间为 1.484–1.627 秒，但实机运动有明显顿挫。
- `50 ms, Kp=0.125, Kd=0` 四向试验完成，无过冲、振荡或丢目标；Yaw 稳定时间 4.035–4.145 秒，Pitch 为 2.613–3.181 秒。
- 用户确认 50 ms 版本的连续性比 200 ms 版本更好，随后在固定 50 ms 周期下继续搜索 PD 参数。

## 未处理事项

- 50 ms 周期基线已经建立；后续参数搜索另见 `2026-07-15_pd-tuning-at-50ms.md`。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\pd_tuning_logs\20260715-185324_kp050_kd000\summary.json`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\pd_tuning_logs\20260715-185826_period050_kp0125_kd000\summary.json`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: Keil UV4 与 Keil MCP Debug `G` 脚本，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
