# 2026-07-15 增加目标短时外推与双轴搜索扫描

## 修改目标

- 目标高速移动造成短时丢失时，继续使用最后一帧 `dx/dy` 修正云台。
- 目标持续不可见时，让双轴在有限角度内执行三角波搜索，提高重新捕获概率。

## 修改内容

1. 新增独立 `target_recovery_control` 状态机。
   - `TRACKING`：使用当前视觉误差执行原 PD 跟踪。
   - `PREDICTING`：目标失效后的 300 ms 内沿用最后有效 `dx/dy`。
   - `SCANNING`：超过 300 ms 后停止旧跟踪命令，执行双轴扫描。
2. 扫描范围按进入扫描时的位置计算。
   - Yaw/Pitch 均为 `-60°..+60°`，总跨度 120°。
   - 按 `3200 pulse/rev` 换算，单侧软范围为 533 pulse。
   - Yaw 每次 4 pulse，Pitch 每次 12 pulse；Pitch 理论扫描频率约为 Yaw 的 3 倍。
3. 两轴拥有独立扫描位置、方向和发送时间戳。
   - 任一轴 UART 忙不会阻塞或推迟另一轴的发送状态。
   - 只有命令成功进入该轴发送队列后，才更新该轴的软位置。
4. 目标重新出现时退出外推/扫描，重置 PD 微分历史并恢复实时跟踪。
5. `TV` 诊断行新增 `RM` 字段：0 跟踪、1 外推、2 扫描。
6. 将新源文件加入 Keil `StepperMotor` 工程组。

## 涉及文件

- `main.c`
- `target_recovery_control.h`
- `target_recovery_control.c`
- `keil/empty_LP_MSPM0G3507_nortos_keil.uvprojx`
- `.log/change/2026-07-15_add-target-loss-recovery-and-search-scan.md`

## 验证情况

- Keil 全量构建通过：0 Error、0 Warning。
- 固件尺寸：Code 9376 B，RO-data 400 B，ZI-data 5088 B。
- 已检查两轴扫描边界钳位、方向反转、命令被拒绝时不累计软位置，以及扫描后重新捕获时的 PD 状态复位。
- 尚未下载到实机；120° 扫描需要先确认机械余量。

## 未处理事项

- 主控没有绝对编码器零位或限位开关。120° 是相对进入扫描时位置的软范围；如果丢失发生在机械边缘，仍可能超出机构允许范围。
- 角度换算假定电机输出轴与云台轴传动比为 1:1；若存在同步带、齿轮或减速结构，需要将传动比加入脉冲换算。
- 当前状态机由视觉观测帧驱动。MaixCAM 正常发送“目标无效帧”时功能成立；若整个视觉 UART 停止发送，暂未加入独立通信看门狗扫描入口。
- 需要实机验证 Pitch 三倍频率是否会造成摄像机线束拉扯、共振或目标重复丢失。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\zdt_x42s.h`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: Keil UV4，command `UV4.exe -b ...uvprojx -j0`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
