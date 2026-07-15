# 2026-07-15 调整搜索范围并再次提高扫描速度

## 修改目标

- 将搜索范围改为 Yaw 左右各 90°、Pitch 上下各 30°，并缩短单次覆盖范围所需时间。

## 修改内容

1. Yaw 单侧软范围从 60°改为 90°，总跨度从 120°改为 180°。
2. Pitch 单侧软范围从 60°改为 30°，总跨度从 120°改为 60°。
3. Yaw/Pitch 单次步幅统一为 24 pulse，扫描调度周期仍为 40 ms。
4. 两轴角速度相同；由于 Pitch 振幅是 Yaw 的 1/3，其完整往返频率约为 Yaw 的 3 倍。
5. 按 3200 pulse/rev 和约 22.5 次/秒有效命令估算：
   - 两轴角速度约 60°/s。
   - Yaw 跨越 180°约 3 秒。
   - Pitch 跨越 60°约 1 秒。

## 涉及文件

- `main.c`
- `.log/change/2026-07-15_adjust-search-range-and-speed.md`

## 验证情况

- Keil 全量构建通过：0 Error、0 Warning；Code 9376 B、RO-data 400 B、ZI-data 5088 B。
- 编译期换算结果：Yaw 单侧 800 pulse，Pitch 单侧 267 pulse。
- 已通过 Keil MCP Debug `G` 脚本下载到 Flash 并直接运行。

## 未处理事项

- 实际速度受视觉无效帧频率、电机队列接受率和机构负载影响，需要实机确认。
- 范围仍以进入扫描时的位置为中心，不是绝对机械零位。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\change\2026-07-15_increase-target-search-scan-speed.md`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: Keil UV4，command `UV4.exe -b ...uvprojx -j0`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: Keil MCP Debug `G` 脚本，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
