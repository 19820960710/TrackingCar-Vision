# 2026-07-15 以 50 ms 周期搜索双轴 PD 参数

## 修改目标

- 在保持 20 Hz 控制更新率的前提下，寻找无持续振荡、回正较快的双轴 PD 参数，并为后续 25 ms 周期试验建立基准。

## 单变量试验

1. 双轴 `Kp=0.18, Kd=0`：Yaw 2.875–3.098 秒，Pitch 1.765–2.116 秒，无过冲。
2. 双轴 `Kp=0.25, Kd=0`：Yaw 1.958–2.105 秒，Pitch 1.183–1.373 秒；Pitch 负向过冲 1 像素。
3. 双轴 `Kp=0.32, Kd=0`：Yaw 1.576–1.581 秒，Pitch 0.960–1.475 秒，无过冲。
4. 双轴 `Kp=0.40, Kd=0`：Yaw 1.206–1.403 秒，Pitch 0.759–0.822 秒；Pitch 负向过冲 1 像素。
5. 双轴 `Kp=0.50, Kd=0`：Yaw 0.941–0.968 秒且无过冲；Pitch 0.546–0.576 秒，但过冲 2–3 像素。
6. 保持 `Kp=0.50`，仅设 Pitch `Kd=0.02`：Pitch 过冲降至 0–1 像素，但正向稳定时间增至 1.264 秒，阻尼偏大。
7. 保持 `Kp=0.50`，仅将 Pitch `Kd` 降至 0.01：Yaw 0.955–1.000 秒，Pitch 0.595–0.597 秒；最多过冲 1 像素。

## 阶段结论

- 50 ms 周期的当前最佳实测参数为：Yaw `Kp=0.50, Kd=0`；Pitch `Kp=0.50, Kd=0.01`。
- 没有观察到丢目标、持续振荡或无效视觉帧。
- D 项应按轴独立设置；Yaw 没有可见过冲，不需要为了形式完整而增加 D。

## 25 ms 初步试验

- 将周期从 50 ms 改为 25 ms，并把双轴 `Kp` 等效减半到 0.25、Pitch `Kd` 减半到 0.005。
- 前三项有效结果：Yaw 1.646–1.789 秒，Pitch 正向 1.202 秒，均无过冲。
- 第四项开始前视觉连续无效，诊断为 `valid=0, dx=-160, dy=-120`；测试保持在 `WAIT_CENTER`，未阻塞、未安全中止。
- 25 ms 下直接按周期减半参数会受到整数脉冲量化影响，响应慢于 50 ms 最佳值；目标恢复后需要在 25 ms 下重新搜索增益。

## 25 ms 完整搜索与最终参数

1. 遮挡排除后重跑 `Kp=0.25`、Pitch `Kd=0.005`：Yaw 1.792–2.351 秒，Pitch 1.239–1.267 秒；无持续振荡，但响应偏慢。
2. 仅将双轴 `Kp` 提至 0.40：Yaw 1.060–1.169 秒，Pitch 0.579–0.622 秒；最大过冲分别为 2 像素和 1 像素。
3. 仅将双轴 `Kp` 提至 0.50：首次因视觉瞬时丢失安全中止；相同代码重跑完成，Yaw 0.789–0.831 秒，Pitch 0.465–0.806 秒，但最大过冲扩大至 3 像素和 5 像素。
4. 最终选择 25 ms、Yaw `Kp=0.40, Kd=0`、Pitch `Kp=0.40, Kd=0.005`，以约 0.2–0.3 秒的速度差换取更低过冲。
5. 关闭 `GIMBAL_PD_TUNING_ENABLED`，恢复正常视觉追踪模式；重新编译并通过 Debug `G` 下载运行。
6. COM11 连续监听 5 秒收到 224 行正常 `TV` 帧；中心误差为 `dx=-2, dy=0..1`，均在死区内，`CS,0,0` 符合预期。

## 涉及文件

- `main.c`
- `tools/pd_tuning_logs/20260715-190214_period050_kp0180_kd000/`
- `tools/pd_tuning_logs/20260715-190414_period050_kp0250_kd000/`
- `tools/pd_tuning_logs/20260715-190610_period050_kp0320_kd000/`
- `tools/pd_tuning_logs/20260715-190818_period050_kp0400_kd000/`
- `tools/pd_tuning_logs/20260715-191009_period050_kp0500_kd000/`
- `tools/pd_tuning_logs/20260715-191208_period050_ykp0500_ykd000_pkp0500_pkd0020/`
- `tools/pd_tuning_logs/20260715-191411_period050_ykp0500_ykd000_pkp0500_pkd0010/`
- `tools/pd_tuning_logs/20260715-191802_period025_ykp0250_ykd000_pkp0250_pkd0005/`
- `tools/pd_tuning_logs/20260715-192614_period025_ykp0250_ykd000_pkp0250_pkd0005_rerun/`
- `tools/pd_tuning_logs/20260715-192811_period025_ykp0400_ykd000_pkp0400_pkd0005/`
- `tools/pd_tuning_logs/20260715-193016_period025_ykp0500_ykd000_pkp0500_pkd0005/`
- `tools/pd_tuning_logs/20260715-193159_period025_ykp0500_ykd000_pkp0500_pkd0005_rerun/`

## 验证方式

- Keil 全量编译后通过 Debug `G` 脚本下载并运行。
- COM11、115200 8N1 连续采集 `PD` 帧；每组依次执行 Yaw 正负、Pitch 正负回正试验。
- 依据稳定时间、IAE、过冲、过零次数和无效帧比较参数。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: Keil UV4 与 Keil MCP Debug `G` 脚本，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `tools/pd_tuning_capture.py`，COM11
