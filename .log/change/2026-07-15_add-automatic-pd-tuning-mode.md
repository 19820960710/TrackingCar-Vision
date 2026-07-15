# 2026-07-15 增加云台自动 PD 调参模式

## 修改目标

- 由固件自动制造画面内的单轴像素偏差，释放闭环回正，并由电脑自动采集和评价误差响应。

## 修改内容

1. 将双轴控制器扩展为独立 `Kp/Kd`，微分项按实际毫秒间隔归一化；本轮基线 `Kd=0`。
2. 增加非阻塞自动调参状态机：中心稳定、扰动、释放、冷却、完成和中止。
3. 扰动终点由视觉误差决定：Yaw ±60 像素，Pitch ±40 像素；不使用固定机械角度。
4. 目标丢失、扰动超时或回正超时时进入中止状态，并分别向双轴提交立即停止。
5. 调参期间输出 `PD` 诊断帧，包含时间、状态、试验编号、有效性、dx/dy、双轴 P/D 项和实际脉冲输出。
6. 新增 `tools/pd_tuning_capture.py`，只读 COM 串口并保存原始日志、CSV 和 JSON 评分。

## 涉及文件

- `main.c`
- `pitch_tracker_control.c`
- `pitch_tracker_control.h`
- `tools/pd_tuning_capture.py`
- `.log/change/2026-07-15_add-automatic-pd-tuning-mode.md`

## 验证情况

- COM11 基线监听 12 秒收到 541 帧 `TV`，约 45.1 Hz；静态误差约为 `dx=3~4`、`dy=-2~-3`。
- PD 调参固件已通过 Keil 全量编译，结果为 `0 Error(s), 0 Warning(s)`；加入采集脚本后的最终构建与烧录将在执行后补充。
- 自动扰动与回正仍需实机验证。

## 未处理事项

- 当前只执行纯 P 基线，`Kd=0`；参数搜索将在基线评分后逐项进行。
- Keil MCP 已配置在 `C:\Users\Aupassen\Desktop\视觉\.mcp.json`，但当前任务未暴露其工具；本轮使用相同的 UV4 命令行后端。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\maixcam_uart_simulator.py`
- Tool: `functions.shell_command`，COM11 基线监听和 Keil UV4 构建，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
