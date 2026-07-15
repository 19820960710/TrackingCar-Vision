# 2026-07-14 双电机非阻塞上电自检

## 修改目标

- 将双电机上电自检从 `delay_cycles()` 阻塞流程改为可在主循环中并发运行的状态机，为后续视觉接收和双轴追踪保留 CPU 时间。

## 修改内容

1. 在 `pitch_motor_control` 中新增 `pitch_motor_commissioning_t` 上电自检上下文及启动、更新接口。
2. 删除旧的阻塞式自检接口；新状态机按毫秒计时，不调用 `delay_cycles()`。
3. PB6 与 PA23 两个电机各自拥有一个状态机，均从同一时刻开始：等待 2500 ms、发送正向 1600 脉冲、等待 2500 ms、发送反向 1600 脉冲、等待 2500 ms 后完成。
4. 主循环继续执行 LED 心跳、视觉 UART 处理和两路电机回包轮询。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.h`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`

## 验证情况

- 使用 TI Arm Clang 编译主程序、生成配置、步进电机和视觉模块源文件，编译通过。
- 未实机验证。烧录后预期 LED 从上电起持续闪烁；约 2.5 秒后两轴同时正向，再约 2.5 秒后同时反向。整个过程中主循环不会因自检延时停止。

## 未处理事项

- 底层每次 UART 命令发送仍为短时同步发送；本次只移除了毫秒级/秒级的阻塞延时。
- 视觉跟踪尚未调用 `pitch_tracker_update()`，本次按要求未接入 MaixCAM 控制。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.h`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- Tool: `functions.exec`，命令 `C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
