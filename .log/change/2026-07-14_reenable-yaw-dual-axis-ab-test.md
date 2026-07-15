# 2026-07-14 重新启用Yaw双轴A/B实验

## 修改目标

- 在pitch单轴视觉闭环已通过实机验证的基础上，仅重新启用yaw跟踪，判断pitch故障是否只在双轴同时控制时出现。

## 修改内容

1. 将`YAW_TRACKING_ENABLED`从`0U`改为`1U`。
   - `PITCH_TRACKING_ENABLED`继续保持`1U`。
   - 未改变UART绑定、物理轴映射、方向、死区、比例、脉冲上限、速度、加速度、发送周期和发送服务顺序。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 使用TI ARM Clang对`main.c`进行编译检查，退出码为0。
- 静态确认yaw和pitch视觉跟踪编译开关均为`1U`。
- 尚未完成本版本实机验证。需要与上一版pitch单轴结果进行A/B比较。

## 未处理事项

- 若重新出现yaw动作而pitch停止，尚不能直接认定底层UART硬件冲突；需要进一步记录两轴命令提交结果、TX忙状态和超时计数，确定冲突发生在控制器还是发送状态机。
- 未处理运行一段时间后主循环停止的问题。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\change\2026-07-14_pitch-only-visual-closed-loop.md`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.shell_command`，command `tiarmclang.exe -mcpu=cortex-m0plus -D__MSPM0G3507__ ... -c main.c`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
