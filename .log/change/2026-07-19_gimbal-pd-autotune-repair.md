# 2026-07-19 云台视觉 PD 自动调参器修复

## 修改目标

- 修复 yaw/pitch 视觉 PD 自动调参器，使其能够安全执行粗搜、细搜和双随机种子复测。
- 本阶段只评价矩形中心回中，不处理激光视差和小车运动状态下的动态跟踪。

## 修改内容

1. 重构 MCU 调参邮箱和状态机。
   - 轨迹缓存溢出改为非致命诊断标志。
   - 分轴记录稳定时间、稳定窗口平均误差、过零次数和最大脉冲跳变。
   - 修正负脉冲被无符号变量解释的问题。
   - 增加命令中止字段、目标有效性预检字段和奇偶快照保护。
   - 超时、状态错误、运动提交错误和用户中止均清空待执行运动并禁用两轴。
2. 增加独立功能开关 `GIMBAL_AUTOTUNE_ENABLED`。
   - 只有视觉跟踪与自动调参同时启用时才创建调参任务。
3. 重写电脑端 `autotune.py`。
   - 支持外部构建/烧录后的 `--skip-build-flash` 模式。
   - 实现 yaw/pitch 分轴粗搜、细搜、双种子 16 次复测和验收阈值。
   - 增加离线自检、靶标预检、异常中止、CSV/JSON/曲线和失败记录。
4. 修复 Keil 工程的 CMSIS 头文件路径。
   - 将不存在的 D 盘 CMSIS 路径替换为本机 `C:\ti\mspm0_sdk_2_05_01_00` 路径。
5. 建立隔离运行环境。
   - Python 环境：`C:\Users\Aupassen\.codex\venvs\gimbal-autotune`。
   - 安装 `pyocd 0.45.0`、`matplotlib 3.11.1` 并写入依赖文件。

## 涉及文件

- `C:\Users\Aupassen\Desktop\M0_Templant_FreeRTOS\Component\app\gimbal_autotune.c`
- `C:\Users\Aupassen\Desktop\M0_Templant_FreeRTOS\Component\app\gimbal_autotune.h`
- `C:\Users\Aupassen\Desktop\M0_Templant_FreeRTOS\Component\config\gimbal_autotune_config.h`
- `C:\Users\Aupassen\Desktop\M0_Templant_FreeRTOS\Component\service\stepper_service.c`
- `C:\Users\Aupassen\Desktop\M0_Templant_FreeRTOS\tools\gimbal_autotune\autotune.py`
- `C:\Users\Aupassen\Desktop\M0_Templant_FreeRTOS\keil\M0_Templant_FreeRTOS.uvprojx`

## 验证情况

- Python `py_compile` 通过。
- `autotune.py --self-test` 通过，Python 邮箱大小为 2456 字节。
- Keil 完整重建通过：`0 Error(s), 0 Warning(s)`。
- map 中 `g_gimbal_autotune` 大小为 2456 字节，与 Python 一致。
- 总 RW 使用 30208 字节（29.50 KiB），未扩大 64 条轨迹缓存。
- `git diff --check` 通过；仅报告工作区既有 CRLF 转换提示。
- 实机烧录未完成：pyOCD 未发现 CMSIS-DAP 探针；Keil 下载返回 `Flash Download failed - Target DLL has been cancelled`。

## 未处理事项

- 尚未运行实机粗搜、细搜和双种子复测，因此没有生成或写回新的 `Kp/Kd`。
- 调试器连接恢复后，应先确认靶标持续有效和云台双向安全行程，再烧录并运行脚本。
- 激光距离补偿和循迹拐弯动态验证按阶段边界暂不处理。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\M0_Templant_FreeRTOS\tools\gimbal_autotune\autotune.py`
- Source: `C:\Users\Aupassen\Desktop\M0_Templant_FreeRTOS\Component\app\gimbal_autotune.c`

## 2026-07-19 实机续测

### Yaw 保护故障与停止逻辑修复

- 用户观察确认：-383 pulses 探测期间 yaw 轴没有转动，随后保持力消失。
- RAM 状态显示 yaw 最后响应为 protection error；软件 `enabled=true` 仅代表已提交使能，不能证明驱动器已退出保护。
- 根因之一是旧的安全停止实现通过“失能后重新使能”刹停；驱动处于保护状态时重新使能会被拒绝，最终没有保持力。
- 按 X42S 协议新增 `0xFE 0x98` 立即停止命令。`stepper_service_stop_all()` 现在清除待执行运动后，对两轴发送立即停止，不再发送失能命令。
- 修复后 Keil 完整重建通过：0 error、0 warning。为避免在保护状态下继续动作，本次只构建，尚未烧录。

- 修正 Keil DFP/Flash Algorithm 后，完整重建与烧录均成功；最新构建为 0 error、0 warning。
- 首次粗搜结果保存在 `gimbal-autotune-results/20260719-182925`。9 个 yaw 候选均因扰动未使图像离开死区而被淘汰，没有写回默认 Kp/Kd。
- 将调参试验改为真正的分轴扰动：yaw 搜索不再同时移动 pitch，反之亦然；失败后清空运动队列，并重新使能两轴以保留保持力。
- 增加扰动幅度上限校验、扰动后实际像素误差记录，以及首个扰动失败时立即停止搜索。
- 先后执行 yaw 单次探测：-204 pulses、-383 pulses（后者等待 1.5 s）。图像横向误差分别仍为 4 pixels、2 pixels，均未离开 ±4 pixels 死区。
- 驱动器返回 `01 FD 02 6B`，表示位置命令已接收；设备配置为仅返回接收确认时，不会返回 `01 FD 9F 6B` 到位帧。因此不能再以 0x9F 作为运动成功的必要条件。
- 当前位置命令帧格式与设备协议一致：0xFD、relative mode=0、sync=0。由于 383 pulses 仍未产生可测的光学位移，正式粗搜暂缓，等待确认 yaw 电机轴/云台是否发生了物理转动。
- Python 离线自检仍通过，mailbox size 为 2456 bytes。
- Tool: `mcp__keil5__keil_build`，完整重建 Keil 工程
- Tool: `mcp__keil5__keil_flash`，尝试烧录并记录硬件连接失败
- Tool: `shell_command`，运行 Python 自检、依赖安装、pyOCD 探针检查和 Git 差异检查，cwd `C:\Users\Aupassen\Desktop\M0_Templant_FreeRTOS`

## 2026-07-19 GitHub 云台驱动严格复核

- 重新从 `origin` 获取并核对了 `云台调试代码` 分支；远端提交仍为 `52bb777`，与本地分支头一致。
- 确认集成分支的硬件命名为 yaw=`UART2/PA23-PA24`、pitch=`UART1/PB6-PB7`。撤销此前依据另一工程所做的两轴 UART 互换，恢复该映射及对应中断分发。
- 发现 `52bb777` 集成代码把位置模式写成了 `0`，但同一 GitHub 仓库的 `gimbal-uart-diagnostic` 实机 bring-up 驱动明确使用 `EMM_POSITION_RELATIVE_CURRENT=2`；本地可运行的 `tracking_vision` 也使用模式 `2`。最终采用“正确 UART 映射 + 模式 2”的组合，并新增具名常量 `ZDT_X42S_MOTION_RELATIVE_CURRENT`。
- 修复后 Keil 完整重建通过：0 error、0 warning；烧录成功。
- 单次 yaw 受限诊断使用 60 RPM、120 脉冲。电机实时位置由 463 变为 449，变化约 14 度，与 120/3200 圈对应的 13.5 度相符，证明位置命令已被执行。
- 该试验仍被调参器以状态 5 淘汰，因为视觉误差没有离开 ±4 像素死区。随后只读采样显示 `g_vision_tracking_debug.uart_packet_count=0`、`last_target_valid=0`，因此未继续参数搜索。
- 原 `.venv` 指向已失效的 `D:\py\python\python.exe`；新建 `.venv314` 并按 `tools/gimbal_autotune/requirements.txt` 安装 `pyocd==0.45.0`、`matplotlib==3.11.1`，供后续脚本运行。
