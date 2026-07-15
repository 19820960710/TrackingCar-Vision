# 2026-07-15 双步进电机不能同时转根因审查

## 审查结论

双轴控制链路本身可以并行运行。此前“只有一个轴转、运行一段时间后全部停止”的主要软件根因是 Keil 启动文件仅分配 256 B 栈，低于静态报告中的已知需求；视觉和诊断格式化输出触发栈越界后，主循环与 LED 心跳停止。实际轴接线与软件实例绑定反置，则造成了早期对 Yaw/Pitch 行为的错误归因。

将栈扩大为 4 KiB、修正轴绑定后，100 Hz、250 Hz 和三轮连续测试均完成，最终 Dual 阶段实机确认两轴同时旋转。

## 证据链

1. 256 B 栈且启用 MS 诊断时，收到 `VISION_READY` 后很快停止，LED 心跳同时停止。
2. 256 B 栈关闭 MS 后，能够输出一段 TV，但约数百帧后仍停止；故障时间随格式化调用负载变化。
3. 仅将栈扩大为 4 KiB、保持原调度逻辑后，100 Hz 单轮完整运行且无停止。
4. 重新启用 MS 后，100 Hz 与 250 Hz 均完整运行，两轴的 Q/F/A 计数一致，J/T/P/E 全为零。
5. 100 Hz 连续三轮最终计数为：
   - Yaw：`Q=122,J=0,F=123,T=0,A=123,R=0,P=0,E=0`
   - Pitch：`Q=122,J=0,F=123,T=0,A=123,R=0,P=0,E=0`
6. 用户在最终 Dual 阶段观察到两个电机同时旋转。

## 猜想处置

### 已确认

- 栈空间不足：确认是导致随机停止、诊断开启后更快停止的根因。
- 轴绑定错误：确认软件轴名与实际 PB6/PB7、PA23/PA24 接线曾经相反，已修正。

### 本轮证据不支持

- 两路电机 UART 必须阻塞或必须串行等待：不支持。当前两轴独立 `service_tx` 即可同时运动。
- 视觉频率过高必然饿死电机调度：不支持。250 Hz 测试完整通过。
- 双轴命令互相覆盖或其中一轴队列长期忙：不支持。两轴 `J=0`，Q/F/A 对称。
- UART 发送超时、返回协议错误或电机保护：不支持。两轴 `T/P/E=0`。
- 电源瞬时电流不足：本轮没有出现与负载相关的失败，且修复纯软件栈配置后双轴已同时运动；不作为当前根因。

## 返回值说明

- `A=F`：所有完成发送的帧都收到了电机“接受”应答。
- `R=0`：当前电机固件/模式没有返回“到位”帧；这不代表命令失败，因为接受计数完整且机械运动已观察到。
- `F=Q+1`：多出的 1 帧是初始化使能帧。

## 保留风险与后续建议

- 4 KiB 栈解决了当前已知负载，但后续增加巡线、显示或更多格式化函数后仍应重新查看 Keil `.htm` 调用图和 RAM 占用。
- 正式比赛版本建议降低逐帧 TV 日志频率，MS 诊断改为编译开关；这是降低串口负载，不是本次双轴故障的必要修复。
- 当前烧录后需按板上 RESET 或完成一次真正的 MCU 断电复位；仅关闭电机电源不一定会使 XDS110 供电下的 MCU 复位。

## 测试产物

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\test_logs\20260715-stack-4k-clean-ready-100Hz\20260715-144515_100Hz\summary.json`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\test_logs\20260715-stack-4k-ms-enabled-100Hz\20260715-144853_100Hz\summary.json`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\test_logs\20260715-stack-4k-ms-enabled-250Hz\20260715-145157_250Hz\summary.json`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\test_logs\20260715-final-3cycles-100Hz\20260715-145659_100Hz\summary.json`

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: Keil linker调用图与栈需求报告
- Source: 串口仿真原始日志及 JSON 统计
- Source: 用户实机运动观察
