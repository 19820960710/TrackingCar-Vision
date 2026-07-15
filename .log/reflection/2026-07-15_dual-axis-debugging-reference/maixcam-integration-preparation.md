# 2026-07-15 MaixCAM 与双轴云台接入准备

## 背景

- 电脑通过 PA0/PA1 仿真 MaixCAM 时，100 Hz 连续椭圆轨迹已驱动两轴同时运行。
- 下一阶段把电脑数据源替换为真实 MaixCAM，验证物理闭环方向并调参。

## 事实记录

- 当前视觉 UART：115200、8N1，PA1 为 MSPM0 RX，PA0 为 MSPM0 TX。
- 当前解析器已通过以下格式的连续输入：`AIM,0,0,0,x,y,0,0,blob-fallback,NO_LASER\n`。
- 当前画面配置为 512×320，中心为 `(256,160)`；相机常见中心 `(255,159)` 位于当前 4 像素死区内。
- 当前 Yaw 为 PB6/PB7，Pitch 为 PA23/PA24。
- 当前测试参数为死区 4 像素、8 脉冲/像素、单次最多 400 脉冲、命令周期 200 ms、速度 60 RPM、加速度 20。
- 当前方向配置：Yaw 正误差映射为反向，Pitch 正误差映射为正向；该配置只通过电脑仿真验证了命令产生，未验证真实机械反馈符号。

## 原因分析

- 电脑预设 dx/dy 不会随电机运动改变，因此只能验证数据链路和双轴调度，不能验证闭环稳定性。
- 真实相机中，电机转动会改变下一帧目标坐标。若方向错误，误差会越来越大，形成正反馈。
- 当前 8 脉冲/像素、最大 400 脉冲是观察用大幅参数；在方向未确认前风险过高。

## 经典坑或可复用经验

- 相机 TX 接 PA1，必须共地；PA0 回传可接相机 RX，也可只接 USB-TTL RX 进行被动监听。
- 允许一个 TX 驱动多个 RX，但禁止把相机 TX 和 USB-TTL TX 同时接到 PA1。
- MaixCAM 必须持续发送。主控在双轴自检完成后才启用视觉 UART，自检前的一次性帧会被丢弃。
- 方向正确的判据不是“电机顺时针”，而是下一帧 `|dx|` 或 `|dy|` 减小。

## 流程调整

### 接入前

1. 确认 MaixCAM 输出为 3.3 V TTL、115200、8N1，并以 `\n` 结束。
2. 接线：MaixCAM TX → PA1，GND → GND；调试阶段 PA0 → USB-TTL RX，被动观察 TV/MS。
3. 将跟踪参数临时降为较小值：死区 8～12 像素、0.5～1 脉冲/像素、单次最多 30～60 脉冲，命令周期保持 200 ms。
4. 保留 PB22 心跳和 MS 计数；确保机械行程允许小幅测试。

### 方向标定

1. 固定目标放在画面右侧，只允许 Yaw 更新；观察第一次运动后 dx 是否趋近 0。
2. 若 `|dx|` 增大，只翻转 Yaw 的 `positive_error_is_cw`。
3. 固定目标放在画面下方，只允许 Pitch 更新；像素 y 向下为正，观察 dy 是否趋近 0。
4. 若 `|dy|` 增大，只翻转 Pitch 的 `positive_error_is_cw`。
5. 再验证左侧和上方，确保正负误差均能回中。
6. 最后开启双轴，目标依次放在四个象限，确认 dx、dy 同时收敛。

### 初始跟踪验收

- 连续收到 AIM，TV 的 valid 为 1，x/y 与相机画面一致。
- 目标静止在中心时两轴不抖动。
- 目标缓慢移动时误差总体减小，不持续同向累积。
- 目标丢失时两轴停止下发新运动命令。
- LED 持续闪烁，两轴 J/T/P/E 不增长。
- 测量实际相机帧率；若高于预期，仍应以固定 200 ms 控制周期运行，不按每帧都发电机命令。

## 栈峰值是否现在测

- 现在接相机前不必先阻塞在栈水位测量上。当前静态已知最大值为 404 B，已分配 4096 B，且 100/250 Hz 仿真和长时间双轴运行没有再次卡死。
- 在加入巡线、显示、按键、更多 `snprintf`、新的中断或 RTOS 任务前必须测。原因是这些模块会增加局部变量、函数嵌套和中断压栈，静态报告中的 `Unknown` 无法给出完整上界。
- 测量方法是“栈水位”：启动时把尚未使用的栈区填入固定字节，例如 `0xA5`；运行最重负载后扫描被覆盖到的位置。4 KiB 中被覆盖的最大长度就是实测峰值。
- 建议至少保留 2 倍峰值或 1 KiB 以上余量，并把中断嵌套和异常路径计算在内。若实测峰值接近 3 KiB，应减少大局部数组/格式化调用或增加栈，而不是等待再次随机卡死。

## 不纳入本次调整

- 本文件没有直接修改跟踪参数和方向；需在相机连接、机械安全确认后一次只改一个参数。
- 暂不写 MSP 外部位置 PID。X42S 内部已经闭合电机位置环，MSP 当前需要的是视觉外环和限幅，不是重复闭合电机位置环。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_uart.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\keil\Objects\empty_LP_MSPM0G3507_nortos_keil.htm`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\test_logs\20260715-live-trajectory-retry\20260715-152908_100Hz_trajectory\summary.json`
- Tool: `functions.shell_command`，command `Select-String Maximum Stack Usage`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
