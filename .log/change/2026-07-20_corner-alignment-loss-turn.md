# 2026-07-20 拐角对齐与丢线 90°恢复

## 修改目标

- 补齐上一版未移植的直角/锐角空间检测和编码器约束转弯状态机。
- 将“丢线立即刹车”改为“顺时针旋转约 90°，再次检测仍丢线才刹车”。

## 修改内容

1. 移植原项目拐角控制模块。
   - 加入 `tracker_corner_detector`、`tracker_corner_alignment` 和 `gear_motor_angle_control`。
   - 保留原项目当前直角探测、候选减速、探线、回退、双轮角度转弯、转后扫描和释放流程。
   - 急角执行仍按原配置关闭，只保留急角检测；直角执行开启。
2. 为 FreeRTOS 轮速服务补充累计编码器只读快照。
   - `speed_service` 仍是编码器增量的唯一读取者。
   - 巡线任务读取发布后的左右累计计数，不直接调用 `encoder_get_data()`，避免破坏轮速 PID 的增量窗口。
3. 加入丢线恢复状态机。
   - 只有至少成功跟踪过一次黑线后，丢线才会触发恢复转弯；上电时没有线不会自行旋转。
   - 当前顺时针命令为左轮 `+200 mm/s`、右轮 `-200 mm/s`。
   - 两轮分别累计约 1650 编码器计数，对应按 174 mm 轮距、65 mm 车轮估算的原地 90°。
   - 任一轮提前到位后先停止该轮；两轮均到位后停车并重新检查黑线。
   - 连续两帧重新检测到线后恢复普通巡线；仍无黑线或 100 个控制周期超时则保持刹车，之后检测到线可自动恢复。
4. 扩充调试状态。
   - `g_line_tracking_status.mode` 可区分普通巡线、拐角对齐、丢线顺时针转弯和丢线刹车。
   - 增加转弯左右计数进度、丢线转弯次数、拐角事件数、拐角转弯数和中止数。

## 涉及文件

- `Component/tracker/gear_motor_angle_control.c`
- `Component/tracker/gear_motor_angle_control.h`
- `Component/tracker/tracker_corner_detector.c`
- `Component/tracker/tracker_corner_detector.h`
- `Component/tracker/tracker_corner_alignment.c`
- `Component/tracker/tracker_corner_alignment.h`
- `Component/app/line_tracking.c`
- `Component/app/line_tracking.h`
- `Component/config/line_tracking_config.h`
- `Component/service/speed_service.c`
- `Component/service/speed_service.h`
- `keil/M0_Templant_FreeRTOS.uvprojx`

## 验证情况

- 原 `tracker_corner_alignment.c` 与来源文件逐行一致，共 1451 行；其余算法 C 文件代码一致，仅注释编码/末尾空行存在文本差异。
- Keil ARM Compiler 6.24 完整构建：`0 Error(s), 0 Warning(s)`。
- 镜像占用：Code 62792 B、RO-data 864 B、RW-data 36 B、ZI-data 28596 B。
- 链接报告确认 `tracker_corner_alignment_update` 最大调用深度约 276 B；巡线任务完整调用链最大约 560 B，低于任务分配的 1536 B。
- `uvprojx` XML 可解析，`git diff --check` 未发现空白错误。

## 未处理事项

- 未烧录、未进行真实车体转弯验证。第一次烧录必须悬空车轮或留出足够旋转空间。
- 左 `+200`、右 `-200` 是否对应本车实际顺时针方向需要实测；若方向相反，只交换 `LINE_TRACKING_LOSS_TURN_LEFT_MM_S` 和 `LINE_TRACKING_LOSS_TURN_RIGHT_MM_S` 的符号。
- 1650 计数是几何估算值。轮胎打滑、轮距和有效轮径会造成实际角度偏差，需根据实测按比例修正 `LINE_TRACKING_LOSS_TURN_90_COUNTS`。
- 原直角状态机参数本身仍属于试验参数；编码器分辨率已按当前 FreeRTOS 轮速档案的 2464 counts/rev 适配，尚未验证直角通过率。

## 依据与工具

- Skill: `C:/Users/Aupassen/.codex/skills/c-style/SKILL.md`
- Skill: `C:/Users/Aupassen/.codex/skills/project-logbook/SKILL.md`
- Source: `C:/Users/Aupassen/Desktop/Gear_motor_release_20260720/Gear_motor_source/source/control/tracker_corner_detector.c`
- Source: `C:/Users/Aupassen/Desktop/Gear_motor_release_20260720/Gear_motor_source/source/control/tracker_corner_alignment.c`
- Source: `C:/Users/Aupassen/Desktop/Gear_motor_release_20260720/Gear_motor_source/source/control/gear_motor_angle_control.c`
- Source: `C:/Users/Aupassen/Desktop/Gear_motor_release_20260720/Gear_motor_source/source/config/project_config.h`
- Tool: `functions.apply_patch`，用于所有工程源文件、配置和日志修改。
- Tool: `functions.shell_command`，命令 `D:/Keil/UV4/UV4.exe -b ... -j0`，cwd `C:/Users/Aupassen/Desktop/M0_Templant_FreeRTOS`。
