# 2026-07-15 云台调试故障与假象清单

## 背景

- 本文件记录本阶段出现过的卡住、无输出和单轴运动现象。
- “已确认”表示有代码、计数、波形或实机对照；“未完全确认”不得作为后续项目的固定结论。

## 事实记录

| 现象 | 原因或状态 | 最有判别力的检查 |
|---|---|---|
| Debug 光标停在 `Reset_Handler/BX R0`，LED 仍闪 | 不是程序卡死；内核正在运行或调试器显示启动汇编位置 | 暂停后看 PC 是否进入 `main`，观察 PB22 心跳 |
| 单步时 `g_monotonic_ms=0` | 内核暂停期间 SysTick 不运行 | 点 Run 运行数秒，再暂停查看计数 |
| 下载成功但板子没有动作 | XDS110 下载完成不等于目标一定完成运行复位；外部电机电源断开也不一定让 MCU 断电 | 下载后按板上 RESET，观察 PB22 心跳和自检 |
| PA23 始终约 3.35 V | 两个 testpoint 连锡使 PA23 与 3V3 短路 | 临时改 1 Hz GPIO；测量 PA23 对 3V3 通断 |
| 串口只收到 `00/FF/C0` 或切换瞬间字节 | 当时线路不是有效 UART 波形，开关/浮空/短路瞬态被串口助手解释为字节 | 示波器看空闲高和完整起始位；先做 GPIO 翻转 |
| 明明初始化了 UART 但目标接口无数据 | 早期诊断代码发送到另一个 UART 实例，或观察口与轴对象不一致 | 同时核对对象、实例、TX 引脚和物理轴四列绑定 |
| 摄像机接入后自检被干扰、出现启动残留 | 相机上电持续输出，视觉接收过早参与启动流程 | 自检完成后再初始化视觉 UART，并先清空 FIFO |
| 只有 Yaw 或只有 Pitch 运动 | 软件轴绑定曾与物理接线相反；随后又叠加栈越界造成随机停止 | 单轴固定误差 + 交换物理接口 A/B 试验 + Q/F/A 计数 |
| 运行一段时间后 TV、LED、电机一起停止 | 256 B 栈小于已知静态需求，`snprintf` 和中断叠加后越界 | 关闭格式化日志做 A/B；查看 Keil Maximum Stack Usage |
| 认为双轴 UART 相互阻塞 | 本轮证据不支持；4 KiB 栈下 250 Hz 双轴测试通过 | 对比两轴 Q/J/F/T/A/P/E，检查是否对称 |
| 第一次连续轨迹只运行约 0.5 秒 | 电脑脚本把 Pitch 的瞬时 `E=3` 当成保护并主动结束 | 区分 `P` 与 `E`；查看 Python 原始日志和 summary |
| 用户认为主控没有回复 | COM11 当时由 Python 独占，串口助手无法同时看到；Python 实际收到 VISION_READY/TV/MS | 检查占用 COM 的进程和 Python `serial.log` |
| 第二次 RESET 只能自检，不能复现 16 秒轨迹 | 电脑脚本已完成一轮并退出，MCU 没有内置误差发生器 | 先启动 Python 等待 VISION_READY，再按 RESET |

## 原因分析

- 多个现象同时存在时，机械观察会产生错误归因。例如轴绑定错误会让 dy 驱动“看起来像 Yaw”，栈越界又会让后续另一个轴停止。
- 仅凭“串口助手没有显示”不能证明 MCU 没有发送；还需确认 COM 占用、观察引脚和显示模式。
- 第一次连续测试的 `E=3` 在下一次完整测试中未复现，最终两轴 `E=0`。其具体来源仍未确认，不能写成电机或线路固定故障。

## 经典坑或可复用经验

- 先证明程序活着，再证明引脚能翻，再证明 UART 有波形，最后才检查协议。
- “增加日志后更容易卡”是栈/内存问题的重要信号，不应继续叠加更多 `printf`。
- 自动化停止条件必须与安全等级一致：保护错误立即停；解析错误记录原始数据并设置阈值。
- 电脑模拟器属于外部数据源；MCU RESET 不会重启它。

## 流程调整

- 每次实验记录：固件哈希/构建时间、接线绑定、复位方式、串口占用者、输入序列、最终计数。
- 每次只改变一个变量，并保存成功与失败的 summary.json。
- 出现协议错误时，下一版诊断应保存最近的原始 4 字节帧，而不是只有累计 E。

## 不纳入本次调整

- 没有把第一次 `E=3` 归因于 Pitch RX 接线，因为后续相同硬件完成测试且 `E=0`。
- 没有把电源电流、250 Hz 负载或电机性能作为根因，因为软件栈修复后同一硬件已通过。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\review\2026-07-15_dual-axis-root-cause-review.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\review\2026-07-14_gimbal-uart-send-path-audit.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\test_logs\20260715-live-trajectory\20260715-151834_100Hz_trajectory\summary.json`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\test_logs\20260715-live-trajectory-retry\20260715-152908_100Hz_trajectory\summary.json`
- Source: 用户在 Keil、万用表、串口助手和电机上的实机观察
- Tool: `functions.shell_command`，command `Get-Content summary.json/serial.log`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
