# 2026-07-14 撤销干扰视觉串口的逐帧诊断

## 修改目标

- 恢复 MaixCAM UART 上短格式 `TV` 回传和已有的闭环运行路径。

## 修改内容

1. 撤销上一版在每条 `TV` 末尾加入 `CS`、`R` 字段的诊断输出。
2. 恢复原始 48 字节本地格式缓冲区和 `TV,valid,dx,dy,x,y` 回传格式。
3. 未改动电机 UART 映射、闭环方向、脉冲参数或自检状态机。

## 涉及文件

- `main.c`

## 验证情况

- 使用 TI ARM Clang 对工程应用源文件执行编译检查。
- 需要实机确认烧录后重新连续收到原始格式 `TV` 行。

## 未处理事项

- pitch 视觉误差明显超过死区但无动作的根因仍未定位。后续诊断不得在同一视觉 UART 上逐帧增加阻塞式回传。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
