# 2026-07-18 视觉 PD 追踪接入与调度周期对齐

## 修改目标

- 在 FreeRTOS 功能验证工程中接入 MaixCAM 的 UART3 目标观测，并将旧 `tracking_vision` 工程的双轴视觉 PD 跟踪核心迁入当前 yaw/pitch 步进服务。
- 使用内部 32 MHz SYSOSC 启动，避免板载外部晶振启动不稳定。
- 将视觉观测和步进服务调度延迟降至 1 ms，以接近旧裸机主循环的处理节拍；PD 命令门限维持 25 ms。

## 修改内容

1. 新增 UART3 视觉接收模块与 FreeRTOS 视觉任务。
   - UART3 使用 PB3 作为 RX、PB2 作为 TX，波特率 115200、8N1。
   - 接收链路采用 UART 中断写入 FreeRTOS 队列，由视觉任务解析完整文本行。
   - `AIM,0,0,0,199,105,0,0,TEMPORAL,NO_LASER` 按旧工程协议解析：`TEMPORAL` 表示目标有效，`NO_LASER` 不阻止以目标中心坐标追踪。
2. 新增硬件无关的 `gimbal_pd_tracker` 控制模块。
   - yaw：Kp=0.40 pulses/pixel，Kd=0。
   - pitch：Kp=0.40 pulses/pixel，Kd=0.005 pulse·s/pixel。
   - 死区 4 px、最大单次输出 400 pulses、命令间隔 25 ms、速度 60 RPM、加速度 20。
   - 方向配置沿用旧工程：yaw 正误差为 CCW，pitch 正误差为 CW。
3. 调度周期对齐。
   - `VISION_TRACKING_TASK_PERIOD_MS` 从 10 ms 改为 1 ms。
   - `STEPPER_SERVICE_POLL_PERIOD_MS` 从 10 ms 改为 1 ms。
   - FreeRTOS tick 为 1 kHz，因此以上周期均以实际 1 ms tick 执行。
4. 启动可靠性与功能验证隔离。
   - 增加强覆盖 `SYSCFG_DL_SYSCTL_init()`，使用内部 32 MHz SYSOSC，并重新设置三个 UART 的 115200 分频。
   - 在功能验证阶段关闭 OLED 任务，避免软件 I2C 外设异常阻塞显示任务。
   - 新增 `g_vision_tracking_debug` RAM 快照，包含 UART 状态、dx/dy、PD 各项和命令计数，供 XDS 非停机读取。

## 涉及文件

- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\main.syscfg`
- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\common\clock_startup.c`
- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\FreeRTOSConfig.h`
- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\vision\vision_uart.c`
- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\vision\vision_uart.h`
- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\control\gimbal_pd_tracker.c`
- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\control\gimbal_pd_tracker.h`
- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\app\vision_tracking.c`
- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\app\vision_tracking.h`
- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\config\vision_tracking_config.h`
- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\config\stepper_service_config.h`
- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\service\stepper_service.c`
- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\task\app_tasks.c`
- `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\config\board_feature_config.h`

## 验证情况

- 2026-07-18 使用 Keil Arm Compiler 6 构建：`0 Error(s), 0 Warning(s)`。
- 2026-07-18 使用 TI XDS 下载当前工程：Keil MCP 返回下载成功。
- Keil 断点验证启动阶段已到达 `g_boot_stage=7`，视觉任务已启动且 UART3 初始化完成。
- UART3 的引脚复用已由 SysConfig 生成：PB3=UART3_RX，PB2=UART3_TX；未发现当前工程内其他模块复用这两个引脚。
- 已启动 pyOCD CMSIS-DAP 桥接，并通过 `cortex_m` 通用目标连接 XDS 读取 RAM；当前快照字段值仍需用下一轮真实目标边缘阶跃试验核验，不能据此给出 PD 收敛时间。

## 未处理事项

- 尚未完成“目标从画面边缘回正”的实机阶跃测试；需要人工把实际目标移至画面边缘，连续采集 dx/dy 与输出脉冲后才能依据收敛时间调整 Kp/Kd。
- 当前 yaw 出现摆头，尚未判定是 yaw 方向正反馈、Kp 偏大、相机目标坐标抖动，还是步进机构回差造成；不得把旧工程 commissioning 参数视为最终可用参数。
- 现有 `g_vision_tracking_debug` 的 CMSIS-DAP 连续快照已可连接，但字段有效性仍需结合 Keil 停机读数交叉验证后再作为调参依据。

## 依据与工具

- Skill: `C:\Users\Aupassen\Desktop\project-logbook-skill-2026-07-09\project-logbook-skill-2026-07-09\skills\project-logbook\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\tracking_vision\control\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\tracking_vision\control\pitch_tracker_control.h`
- Source: `C:\Users\Aupassen\Desktop\tracking_vision\vision_comm\vision_packet.c`
- Tool: `mcp__keil5__keil_build`，项目 `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\keil\M0_Templant_FreeRTOS.uvprojx`
- Tool: `mcp__keil5__keil_flash`，项目 `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\keil\M0_Templant_FreeRTOS.uvprojx`
- Tool: `mcp__keil5__keil_debug_script`，项目 `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\keil\M0_Templant_FreeRTOS.uvprojx`
- Tool: `mcp__keil5__keil_collect_cmsis_dap`，项目 `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\keil\M0_Templant_FreeRTOS.uvprojx`
