# 2026-07-14 增加双轴闭环命令诊断

## 修改目标

- 区分 pitch 无动作是控制器未下令，还是已将命令写入 PB6/UART1 但电机端未执行。

## 修改内容

1. 扩展现有 `TV` 调试行：
   - `CS,pitch,yaw`：本帧对应轴是否成功调用了电机发送函数。
   - `R,pitch,yaw`：两轴最近一次解析到的 X42S 应答枚举值。
2. 不改变视觉误差计算、两个 UART 引脚、PID/比例参数、步进电机帧格式或方向。

## 涉及文件

- `main.c`

## 验证情况

- 使用 TI ARM Clang 对工程应用源文件执行编译检查。
- 未实机验证。接收 `TV` 行后，`dy` 明显超过 4 像素时，预期偶尔出现 `CS,1,...`；若始终为 0，则从控制条件排查；若为 1 而 pitch 无动作，则检查 PB6 电机协议执行或其供电/使能状态。

## 未处理事项

- 该诊断通过 MaixCAM UART 回传，会增加调试期串口输出；确认故障后应移除或降低频率。
- yaw 正反馈和机械限位风险未通过本次诊断修复；实机测试需从安全位置开始。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\zdt_x42s.c`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
