# 2026-07-18 yaw PD 第一轮抑制摆头参数试验

## 修改目标

- 针对 yaw 轴在视觉目标跟踪时出现摆头，降低比例输出并加入误差变化率制动项，作为实机收敛测试的第一轮参数。

## 修改内容

1. 修改 yaw 视觉 PD 参数。
   - `VISION_TRACKING_YAW_KP_PULSES_PER_PIXEL`：0.40 → 0.20 pulses/pixel。
   - `VISION_TRACKING_YAW_KD_PULSE_SECONDS_PER_PIXEL`：0 → 0.008 pulse·s/pixel。
   - yaw 的命令间隔仍为 25 ms；视觉任务和步进服务轮询仍为 1 ms。
   - pitch 参数、轴方向、速度 60 RPM、加速度 20 和最大脉冲数未修改。

## 涉及文件

- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\config\vision_tracking_config.h`

## 验证情况

- 2026-07-18 使用 Keil Arm Compiler 6 构建：`0 Error(s), 0 Warning(s)`。
- 2026-07-18 使用 TI XDS 通过 Keil MCP 下载成功。
- 参数的实机收敛速度、超调量和摆头次数尚未测得；本条记录不将该组参数标记为已验证。

## 未处理事项

- 需要在目标从画面边缘移至中心的实际阶跃试验中记录 dx/dy 回正时间，再决定是否增大 Kp 或调整 Kd。
- 若误差在给定方向移动后持续增大，应先修正 yaw 正负方向，不能仅靠调小 Kp/Kd 解决。
- 通用 `cortex_m` pyOCD 读取 MSPM0 RAM 的字段值尚未完成可信度校验，当前不以其输出直接决定参数。

## 依据与工具

- Skill: `C:\Users\Aupassen\Desktop\project-logbook-skill-2026-07-09\project-logbook-skill-2026-07-09\skills\project-logbook\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\tracking_vision\control\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\config\vision_tracking_config.h`
- Tool: `mcp__keil5__keil_build`，项目 `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\keil\M0_Templant_FreeRTOS.uvprojx`
- Tool: `mcp__keil5__keil_flash`，项目 `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\keil\M0_Templant_FreeRTOS.uvprojx`
