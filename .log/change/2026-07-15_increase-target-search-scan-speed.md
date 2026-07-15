# 2026-07-15 提高目标搜索扫描速度

## 修改目标

- 缩短目标持续丢失后双轴覆盖 120° 搜索范围所需的时间，同时不增加双电机 UART 命令频率。

## 修改内容

1. 保持扫描调度周期 40 ms、双轴总跨度 120° 和 Pitch:Yaw 约 3:1 的速度比不变。
2. Yaw 单次扫描步幅从 4 pulse 提高到 12 pulse。
3. Pitch 单次扫描步幅从 12 pulse 提高到 36 pulse。
4. 按 3200 pulse/rev 和约 22.5 次/秒的实际视觉帧驱动频率估算：
   - Yaw 约 30°/s，完整跨越 120° 约 4 秒。
   - Pitch 约 90°/s，完整跨越 120° 约 1.33 秒。

## 涉及文件

- `main.c`
- `.log/change/2026-07-15_increase-target-search-scan-speed.md`

## 验证情况

- Keil 全量构建通过：0 Error、0 Warning；Code 9376 B、RO-data 400 B、ZI-data 5088 B。
- 已通过 Keil MCP Debug `G` 脚本下载到 Flash 并直接运行。
- 扫描范围参数未改变：Yaw 左右各 60°，Pitch 上下各 60°。

## 未处理事项

- 实际扫描速度受 MaixCAM 无效帧输出频率、电机命令接受率和机构负载影响，需要实机观察。
- 主控仍无绝对限位；扫描范围以进入扫描时的位置为中心。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\change\2026-07-15_add-target-loss-recovery-and-search-scan.md`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: Keil UV4，command `UV4.exe -b ...uvprojx -j0`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: Keil MCP Debug `G` 脚本，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
