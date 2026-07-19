# 2026-07-19 云台自动调参无运动静态审查

## 审查结论

自动调参器确实先关闭视觉控制、等待一个视觉命令周期并清空步进命令队列，因此现有证据不支持“视觉追踪把扰动立即抵消”是本次无运动的主因。

主问题位于扰动命令生成与成功判据：调参器曾把 120--400 脉冲的单次扰动拆成 20 脉冲小段，每 25 ms 向 `ZDT_X42S_MOTION_RELATIVE_CURRENT` 模式连续提交。该模式以收到命令时的实时位置为基准，前一段尚未完成时继续改写目标，无法保证累计得到原请求位移。程序又只检查命令是否写入 UART，导致把“已发送”误当作“电机已完成扰动”。

已改为一次性提交有界扰动，并在恢复视觉闭环前通过电机位置查询验证实际轴位移至少达到请求位移的 50%。分轴搜索现在只要求被扰动轴稳定，避免 yaw 搜索被未参与试验的 pitch 误差淘汰。临时上电运动诊断已关闭。

## 证据链

1. `tracking_vision` 的 2026-07-15 最终实机记录确认当时接线为 Yaw=UART1/PB6-PB7、Pitch=UART2/PA23-PA24，并记录约 400 秒双轴连续运动。
2. `功能实验和验证/M0_Templant_FreeRTOS` 与当前工程代码均标注 Yaw=UART2/PA23-PA24、Pitch=UART1/PB6-PB7；其 2026-07-17 映射日志明确写有“尚未烧录和执行实机动作”。因此不能用该日志反驳旧工程的实机记录，也不能在没有确认当前接线的情况下仅凭旧工程直接交换当前映射。
3. 用户此前观察到当前固件在移动车体后能自动回正，证明当前视觉到步进驱动链路曾实际生效；本轮未改 UART 映射。
4. `gimbal-autotune-results/20260719-193426/trials.csv` 记录请求 yaw 扰动 383 脉冲，但位置只从 -3364 变到 -3371，约 7 度；按 3200 脉冲/圈应约为 43.1 度。旧状态机仍把串口提交判为成功，随后只因图像误差未离开死区而失败。
5. 先前一次单帧 120 脉冲诊断记录的位置由 463 变到 449，约 14 度，与理论 13.5 度一致，支持恢复单帧扰动。

## 已修改

- `Component/app/gimbal_autotune.c`
  - 删除 20 脉冲分段扰动，恢复单次有界位置命令。
  - 增加编码器位移比例验收。
  - yaw/pitch 分轴试验只检查活动轴的稳定条件。
- `Component/config/gimbal_autotune_config.h`
  - 关闭上电自动运动诊断。
  - 固化 3200 脉冲/圈和最小 50% 实际位移阈值。

## 验证

- Keil 完整重建：0 Error(s), 0 Warning(s)。
- Python 离线自检：通过，mailbox size=2712 bytes。
- `git diff --check`：无空白错误，仅有工作区既有 LF/CRLF 提示。
- 未烧录、未执行实机动作；电池耗尽期间只做静态检查和构建。

## 后续实机检查顺序

1. 上电并按 Reset 后，先确认正常视觉回正仍有效，以当前实物行为确定轴映射，不按历史标签猜测。
2. 单独执行一次 120 脉冲 yaw 扰动，要求查询位置变化约 13.5 度且图像横向误差离开死区。
3. 上述检查通过后再启动 yaw 粗搜；失败时保存请求脉冲、前后位置和图像误差，不继续候选搜索。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\tracking_vision\.log\reflection\2026-07-15_dual-axis-debugging-reference\README.md`
- Source: `C:\Users\Aupassen\Desktop\tracking_vision\.log\review\2026-07-15_dual-axis-root-cause-review.md`
- Source: `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\.log\change\2026-07-17_zdt-dual-uart-axis-routing.md`
- Source: `C:\Users\Aupassen\Desktop\M0_Templant_FreeRTOS\gimbal-autotune-results\20260719-193426\trials.csv`
- Tool: Keil complete rebuild
- Tool: Python autotune offline self-test
