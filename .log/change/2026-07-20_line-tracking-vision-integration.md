# 2026-07-20 巡线与云台视觉并行融合

## 修改目标

- 将 `Gear_motor_release_20260720` 中的 8 路模拟灰度巡线采样、标定数据和位置控制器融合到当前 FreeRTOS 工程。
- 复用现有编码器、TB6612 和轮速闭环，同时保留视觉任务对 yaw/pitch 云台的独立控制。

## 修改内容

1. 新增巡线传感器驱动和 MSPM0 适配层。
   - PA15 使用 ADC1 通道 0；PB5、PB15、PB16 选择 8 路模拟通道。
   - 每通道采样 8 次，使用原工程的黑白标定数组归一化到 0～1000。
   - `g_line_tracking_status` 保留 8 路原始值、归一化值、峰值、误差和目标速度，便于实机调试器观察。
2. 移植巡线位置控制器和当前启用参数。
   - 30 ms 控制周期，黑线检测进入/退出阈值为 80/60。
   - 使用原工程 active candidate247：基础速度 200 mm/s，`Kp=110`、`Kd=2.5`，并保留边缘增益、导数滤波、限幅、斜率限制和弯道降速参数。
3. 新增 FreeRTOS 巡线任务。
   - 巡线任务只计算左右轮目标并调用 `speed_service_set_target_mm_s()`。
   - 原工程的电机驱动和速度 PID 没有重复移植，现有 10/30 ms 轮速服务继续负责编码器闭环和 PWM。
   - 丢线或 ADC 读取失败时立即写入双轮零速目标。
4. 明确车轮控制权。
   - `LINE_TRACKING_ENABLED=1` 时，车体 yaw 环仍维持定时通知链，但不再覆盖巡线任务写入的左右轮目标。
   - 云台视觉任务和步进电机服务不受该开关影响，可与巡线任务同时运行。
5. 同步 SysConfig 与实际时钟。
   - 在 `main.syscfg` 中加入 ADC 和地址引脚，绑定工程实际使用的 MSPM0 SDK 2.05.01.00。
   - 工程通过强时钟初始化运行在 32 MHz；新增 ADC 强初始化使用实际 16 MHz ULPCLK 范围。
   - 将轮速节拍定时器强初始化为 319999，保证实际 10 ms，而不是按生成文件的 80 MHz 假设运行成 25 ms。

## 涉及文件

- `Component/app/line_tracking.c`
- `Component/app/line_tracking.h`
- `Component/config/line_tracking_config.h`
- `Component/tracker/tracker_sensor_analog_8ch.c`
- `Component/tracker/tracker_sensor_analog_8ch.h`
- `Component/tracker/tracker_analog_8ch_mspm0.c`
- `Component/tracker/tracker_analog_8ch_mspm0.h`
- `Component/tracker/tracker_position_control.c`
- `Component/tracker/tracker_position_control.h`
- `Component/task/app_tasks.c`
- `Component/common/clock_startup.c`
- `main.syscfg`
- `ti_msp_dl_config.c`
- `ti_msp_dl_config.h`
- `keil/M0_Templant_FreeRTOS.uvprojx`

## 验证情况

- 使用 SysConfig 1.26.2 和 MSPM0 SDK 2.05.01.00 重新生成配置，结果为 0 error、1 条 SYSCTL 最佳实践 warning。
- 使用 Keil ARM Compiler 6.24 完整构建：`0 Error(s), 0 Warning(s)`。
- 镜像占用：Code 50520 B、RO-data 708 B、RW-data 36 B、ZI-data 28116 B。
- 链接报告确认 `line_tracking_task`、强 `SYSCFG_DL_ADC12_0_init` 和强 `SYSCFG_DL_TIMER_0_init` 均进入最终镜像。
- 巡线任务最大调用栈深度约 448 B，任务分配 384 words（1536 B）。

## 未处理事项

- 未烧录、未进行真实赛道测试；启用后复位会自动开始巡线，烧录前必须悬空车轮或将车放到安全赛道。
- 未移植原工程的直角/锐角识别与对齐状态机。该部分依赖累计轮位移，且原工程当前 200 mm/s 候选记录为急弯通过率约 50%，需要单独适配和实测。
- PA15/PB5/PB15/PB16 的外部接线方向、传感器左右顺序和当前黑白标定值仍需在本车上确认；若左右接反，转向修正方向也会相反。
- 当前基础速度 200 mm/s 低于现有 MG680 轮速档案记录的 250 mm/s 已验证最低速度，低速闭环效果需要实机确认。

## 依据与工具

- Skill: `C:/Users/Aupassen/.codex/skills/c-style/SKILL.md`
- Skill: `C:/Users/Aupassen/.codex/skills/project-logbook/SKILL.md`
- Source: `C:/Users/Aupassen/Desktop/Gear_motor_release_20260720/Gear_motor_source/main.c`
- Source: `C:/Users/Aupassen/Desktop/Gear_motor_release_20260720/Gear_motor_source/source/config/project_config.h`
- Source: `C:/Users/Aupassen/Desktop/Gear_motor_release_20260720/Gear_motor_source/source/drivers/tracker_sensor/analog_8ch/tracker_sensor_analog_8ch.c`
- Source: `C:/Users/Aupassen/Desktop/Gear_motor_release_20260720/Gear_motor_source/source/platform/tracker_analog_8ch_mspm0.c`
- Source: `C:/Users/Aupassen/Desktop/Gear_motor_release_20260720/Gear_motor_source/source/control/tracker_position_control.c`
- Tool: `functions.shell_command`，命令 `syscfg.bat ... main.syscfg`，cwd `C:/Users/Aupassen/Desktop/M0_Templant_FreeRTOS`
- Tool: `functions.shell_command`，命令 `D:/Keil/UV4/UV4.exe -b ... -j0`，cwd `C:/Users/Aupassen/Desktop/M0_Templant_FreeRTOS`
- Tool: `functions.apply_patch`，用于所有工程源文件和配置修改。
