# 云台视觉工程变更总索引

更新日期：2026-07-15

## 使用方法

本文件是 `.log/change/` 的入口，不替代原始变更记录。原始日志保留每次单变量实验的条件和证据；比赛前移植时应先读本文件，再读对应阶段的关键日志，不能按文件名顺序把历史参数逐项重做。

现行状态以当前源码、SysConfig 和 Keil 工程为准。下面标成“历史试验”的配置已经被后续修改覆盖，不应直接复制到新工程。

## 当前有效配置

| 项目 | 当前值 | 权威位置 |
| --- | --- | --- |
| 主控 | MSPM0G3507，裸机主循环 + SysTick 1 ms | `main.c` |
| MaixCAM | UART0，PA0 TX、PA1 RX，115200 8N1 | `empty.syscfg`、`vision_comm/vision_uart.c` |
| Yaw 电机 | UART1，PB6 TX、PB7 RX，地址 1 | `main.c`、`empty.syscfg` |
| Pitch 电机 | UART2，PA23 TX、PA24 RX，地址 1 | `main.c`、`empty.syscfg` |
| 心跳灯 | PB22，500 ms 翻转 | `main.c`、`empty.syscfg` |
| 图像尺寸 | 320×240，控制中心 `(160,120)` | `vision_comm/vision_config.h` |
| 控制周期 | 25 ms | `main.c` |
| Yaw PD | `Kp=0.40`，`Kd=0`，死区 ±4 px | `main.c` |
| Pitch PD | `Kp=0.40`，`Kd=0.005`，死区 ±4 px | `main.c` |
| 控制方向 | Yaw `positive_error_is_cw=false`；Pitch `true` | `main.c` |
| 单次控制限幅 | 两轴均 400 pulse | `main.c` |
| 电机位置模式 | 相对实时位置，立即执行 | `pitch_motor_control.c` |
| 开机自检 | 两轴同时执行约 +30°、-30°回原位 | `pitch_motor_control.c` |
| 丢失短时预测 | 沿用最后有效 `dx/dy` 300 ms | `target_recovery_control.c`、`main.c` |
| 搜索范围 | Yaw ±90°；Pitch ±30° | `main.c` |
| 搜索步进 | 两轴每 40 ms 请求 24 pulse，60 RPM | `main.c` |
| 启动栈 | 4 KiB | `keil/startup_mspm0g350x_uvision.s` |

注意：扫描范围以“进入扫描状态时的位置”为临时中心，不是人工标定的机械零位。人工设基准功能尚未实现。

## 变更阶段索引

### 1. 工程建立、协议接口和 Keil 集成

- [云台步进电机 UART 起步](2026-07-13_gimbal-stepper-uart-bringup.md)
- [MaixCAM 视觉 UART 与 Keil 集成](2026-07-13_maixcam-vision-uart-keil-integration.md)
- [电机 UART MFCLK 诊断](2026-07-13_motor-uart-mfclk-diagnostic.md)

这一阶段确定了三路 UART 的基础工程结构，并把视觉文本协议归一化为 `vision_observation_t`。新视觉算法只需输出受支持的 `AIM` 或 `TV` 行，云台控制层不应直接依赖 MaixCAM 内部检测算法。

### 2. UART 与硬件通路排障

- [PB22 心跳测试](2026-07-14_pb22-led-heartbeat-test.md)
- [PB6 原始 UART 帧测试](2026-07-14_uart1-pb6-raw-diagnostic.md)
- [PA23 普通 GPIO 翻转测试](2026-07-14_pa23-gpio-toggle-test.md)
- [PA23/UART2 电机测试路由](2026-07-14_pa23-uart2-motor-test-route.md)
- [PA23 最终 UART 测试](2026-07-14_uart-test-final-switch-to-pa23.md)
- 历史换线试验：[PB6](2026-07-14_uart-test-switch-to-pb6.md)、[PA23](2026-07-14_uart-test-switch-back-to-pa23.md)、[再次 PB6](2026-07-14_uart-test-switch-again-to-pb6.md)
- 历史多路输出试验：[PA0/PA1 周期帧](2026-07-14_add-uart0-pa0-pa1-periodic-frame.md)、[双 UART](2026-07-14_dual-uart-periodic-enable-frame.md)、[三 UART](2026-07-14_enable-three-uart-periodic-frame.md)
- [Yaw UART2 周期查询](2026-07-14_yaw-uart2-periodic-query.md)

关键结论：PA23 长期保持 3.3 V 的根因不是 UART 初始化、上下拉或芯片“特殊引脚”限制，而是开发板两个测试点连锡，PA23 被硬短接到 3V3。PB22 心跳证明固件运行，PA23 GPIO 翻转失败把问题定位到硬件网络；清除连锡后 GPIO 与 UART 均恢复。

### 3. 电机协议封装、自检和非阻塞发送

- [替换为已验证 ZDT 参考封装](2026-07-14_replace-uart-with-zdt-reference.md)
- [TX FIFO 发送路径](2026-07-14_gimbal-tx-fifo-send.md)
- [非阻塞 UART TX 与超时](2026-07-14_nonblocking-uart-tx-with-timeout.md)
- [双电机开机自检](2026-07-14_dual-motor-power-on-commissioning-test.md)
- [非阻塞双电机自检](2026-07-14_nonblocking-dual-motor-commissioning.md)
- [双电机自检状态诊断](2026-07-14_dual-motor-commissioning-state-diagnostic.md)
- [自检幅度调试](2026-07-14_tracking-commissioning-amplitude.md)
- [自检限制为 30°](2026-07-15_limit-dual-axis-self-test-to-30-degrees.md)
- 历史单轴试验：[Pitch 使能](2026-07-14_pitch-motor-commissioning-test.md)、[Pitch 180°](2026-07-14_pitch-motor-180-degree-test.md)、[PB6 Pitch 第一步](2026-07-14_pitch-motor-pb6-control-stage1.md)

现行约束：所有 `move/stop/enable` 只负责排队；`service_tx()` 每次主循环推进有限字节；`poll()` 每次主循环读取返回；不得等待 FIFO、等待电机到位或使用固定延时阻塞另一轴。

### 4. MaixCAM 接收、解析与调试回传

- [PA1 接收 AIM 文本](2026-07-14_maixcam-pa1-aim-receive.md)
- [UART0 中继测试](2026-07-14_maixcam-uart0-relay-test.md)
- [RX 诊断计数](2026-07-14_maixcam-rx-diagnostic-counter.md)
- [RX FIFO 阈值调整](2026-07-14_maixcam-rx-fifo-threshold.md)
- [自检完成后再启动 MaixCAM UART](2026-07-14_deferred-maixcam-uart-after-commissioning.md)
- [运行时间/复位诊断](2026-07-14_motor-test-uptime-reset-diagnostic.md)

支持的摄像机实例帧：`AIM,0,0,0,255,159,0,0,blob-fallback,NO_LASER\n`。解析层只发布最新完整观测；主循环消费后计算目标相对画面中心的误差。`VISION_READY` 表示双轴自检结束且视觉 UART 已启用，不表示已经收到有效目标。

### 5. 单轴到双轴闭环

- [Pitch 视觉外环第一步](2026-07-14_pitch-visual-outer-loop-stage1.md)
- [Pitch 单轴闭环](2026-07-14_pitch-only-visual-closed-loop.md)
- [Yaw 单轴闭环](2026-07-14_yaw-only-visual-closed-loop.md)
- [恢复双轴 A/B 实验](2026-07-14_reenable-yaw-dual-axis-ab-test.md)
- [双轴视觉跟踪](2026-07-14_dual-axis-visual-tracking-control.md)
- [交换轴 UART 绑定诊断](2026-07-14_swap-axis-uart-binding-diagnostic.md)
- [交换 TX 服务顺序](2026-07-14_swap-motor-tx-service-order.md)
- [双轴命令诊断](2026-07-14_add-dual-axis-command-diagnostic.md)
- [修复 Yaw 返回轮询](2026-07-14_fix-yaw-uart-response-poll.md)
- [电脑仿真与电机返回诊断](2026-07-15_pc-uart-simulator-and-motor-response-diagnostics.md)
- [连续视觉轨迹](2026-07-15_add-continuous-vision-trajectory.md)
- [双轴栈与绑定修复](2026-07-15_fix-dual-axis-stack-and-binding.md)

最终根因不是两路 UART 天生不能并行。主要软件故障是 Keil 启动文件仅给 256 B 栈，`snprintf`、视觉回传、电机诊断和中断嵌套触发越界；同时早期物理接线与软件轴名反置，干扰了现象判断。扩栈至 4 KiB并修正 Yaw/Pitch 绑定后，100 Hz、250 Hz 和连续三轮测试均完成，两轴可同时运动。

### 6. 比例、方向、中心与 PD 调参

- 历史降幅：[降低 pulse/px](2026-07-15_reduce-tracking-pulses-per-pixel.md)、[进一步降幅](2026-07-15_further-reduce-tracking-amplitude.md)
- [相同双轴限幅](2026-07-14_equalize-axis-tracking-pulse-limit.md)
- 历史慢周期：[200 ms](2026-07-14_slow-visual-motor-command-period.md)、[50 ms](2026-07-15_change-control-period-to-50ms.md)
- [中心停止与目标丢失停止](2026-07-15_stop-motor-on-center-or-target-loss.md)
- [修正画面为 320×240](2026-07-15_correct-vision-frame-to-320x240.md)
- 方向试验：[反转并限制校准](2026-07-14_reverse-yaw-and-limit-calibration-motion.md)、[双轴反转](2026-07-15_reverse-both-visual-feedback-directions.md)、[方向再次修正](2026-07-15_correct-both-visual-feedback-directions.md)、[核对轴后反转](2026-07-15_invert-both-directions-after-axis-check.md)、[最终单独修正 Yaw](2026-07-15_correct-yaw-direction-after-axis-test.md)
- [自动 PD 调参模式](2026-07-15_add-automatic-pd-tuning-mode.md)
- [延长基线观察窗](2026-07-15_extend-pd-baseline-response-window.md)
- 参数试验：[Kp 0.70](2026-07-15_pd-tuning-kp-070.md)、[Kp 0.50](2026-07-15_pd-tuning-kp-050.md)、[Kp 0.35](2026-07-15_pd-tuning-kp-035.md)、[50 ms 调参](2026-07-15_pd-tuning-at-50ms.md)

现行采用 25 ms、Yaw `Kp=0.40/Kd=0`、Pitch `Kp=0.40/Kd=0.005`。方向以“电机动作后 `|dx|/|dy|` 是否持续减小”为判据，不能只按肉眼的左右、上下命名判断。历史上目标被推到右下角的主要原因是把 320×240 图像错当成 512×320，参考中心写成 `(256,160)`。

### 7. 目标丢失恢复与扫描

- [加入惯性延续与搜索扫描](2026-07-15_add-target-loss-recovery-and-search-scan.md)
- [提高搜索速度](2026-07-15_increase-target-search-scan-speed.md)
- [调整扫描范围与速度](2026-07-15_adjust-search-range-and-speed.md)
- [协议告警时继续轨迹](2026-07-15_continue-trajectory-on-protocol-warning.md)

状态定义：`RM=0` 正常跟踪；`RM=1` 在 300 ms 内复用最后有效误差；`RM=2` 扫描。重新发现目标时先停止扫描动作、重置 PD 历史，再进入跟踪，避免导数突变。

## 已过时或仅用于排障的结论

| 历史内容 | 当前处理 |
| --- | --- |
| PA23 不可用、可能受 VREF+ 限制 | 已否定；实际为板上测试点连锡短接 3V3 |
| 给 TX 配下拉 | 不采用；UART 空闲高电平由外设驱动 |
| ASCII 窗口看到乱码等于发送错误 | 不成立；电机协议是二进制，应切 HEX 并核对波特率 |
| 只接 GND 和 MCU TX 即可完成闭环诊断 | 只能看发送；读取电机应答还需电机 TX 接 MCU RX |
| 三路 UART 同时周期发测试字节 | 仅用于引脚排障，正式工程不保留 |
| 1600 pulse/180° 或大角度开机自检 | 已被约 30°自检替代 |
| 200 ms 控制周期 | 已被 25 ms PD 替代；200 ms 会造成明显顿挫 |
| 图像中心 `(256,160)` | 错误；当前是 `(160,120)` |
| 旧方向翻转记录 | 仅是实验时间线；最终值看 `main.c` |
| `Kp=0.70`、`Kp=0.50` | 调参试验，非最终参数 |
| 诊断日志逐视觉帧无限输出 | 比赛版不建议；会占用 UART、栈和主循环时间 |

## 新工程最短移植清单

1. 复制并配置 `vision_comm/`，确保输出统一为 `vision_observation_t`。
2. 复制 `zdt_x42s.*`、`stepper_motor.*`、`pitch_motor_control.*`。
3. 复制 `pitch_tracker_control.*` 和 `target_recovery_control.*`。
4. 在 SysConfig 建立三路 115200 UART，并核对实例、引脚、MFCLK、FIFO 和中断名。
5. 给每个电机建立独立对象、独立 TX 状态和独立 RX 解析状态；禁止共享静态发送索引。
6. 每轮主循环固定执行：视觉收字节 → 发布最新目标 → 两轴 `poll` → 两轴 `service_tx` → 到期控制 → 低优先级诊断。
7. 先做 PB22 心跳、GPIO 引脚翻转、原始 UART 帧、单轴自检、双轴自检，再接视觉闭环。
8. 核对 Keil 启动文件栈，不要把 SDK 默认值当作足够。
9. 首次装机用小角度、小速度、可断电条件标定方向和机械范围。
10. 比赛前关闭高频 `TV/MS/PD` 调试输出，保留计数器和低频状态摘要。

完整的问题因果链、模块接口建议和比赛检查表见 [云台视觉集成复盘](../reflection/2026-07-15_gimbal-vision-integration-retrospective.md)。

## 依据与工具

- Skill：`C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Skill reference：`C:\Users\Aupassen\.codex\skills\project-logbook\references\development-change-log.md`
- Skill：`C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill：`C:\Users\Aupassen\.codex\skills\stop-slop\SKILL.md`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_uart.c`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\target_recovery_control.c`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\change\`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\review\`
- Tool：PowerShell `Get-ChildItem`、`Get-Content`、`Select-String`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
