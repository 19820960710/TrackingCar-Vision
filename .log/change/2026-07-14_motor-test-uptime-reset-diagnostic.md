# 2026-07-14 自检复位判别计时诊断

## 修改目标

- 区分“自检状态机未执行”与“首次运动命令附近发生 MCU 复位”，解释自检状态长期停留在 `CM,0,0` 的现象。

## 修改内容

1. 在现有 VU 诊断行首字段增加 `uptime_ms`。
2. 将 VU 行缓冲区扩展为 64 字节，避免增加计时字段后截断。
3. 新格式为 `VU,uptime_ms,packet_count,parse_error_count,overrun_count,CM,pb6_state,pa23_state`。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 使用 TI Arm Clang 编译主程序、生成配置、步进电机和视觉模块源文件，编译通过。
- 未实机验证。若 `uptime_ms` 循环回到约 1000--2000 ms，表明 MCU 在首次 2500 ms 自检动作附近复位；若 uptime 持续增长但 CM 仍为 0，需要在 Keil 检查状态上下文或执行流。

## 未处理事项

- 该诊断不修复潜在供电复位。若确认复位，需检查电机电源与 MSP 电源是否隔离、地线连接、降压模块容量和启动瞬态。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- Tool: `functions.exec`，命令 `C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
