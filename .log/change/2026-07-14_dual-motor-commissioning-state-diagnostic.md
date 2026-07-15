# 2026-07-14 双电机自检状态诊断

## 修改目标

- 在电机未观察到运动时，确认非阻塞自检状态机是否实际运行至发送正反向命令的分支。

## 修改内容

1. 在现有每秒 `VU` UART0 诊断行末尾追加 `CM,pb6_state,pa23_state`。
2. 状态码：0 为等待起始延时；1 为已发送正向命令、等待；2 为已发送反向命令、等待；3 为自检完成。
3. 不修改电机命令数据、UART 引脚或自检时序。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 使用 TI Arm Clang 编译主程序、生成配置、步进电机和视觉模块源文件，编译通过。
- 未实机验证。复位后预期约 1--2 秒可见 `CM,0,0`，约 3--4 秒为 `CM,1,1`，约 5--6 秒为 `CM,2,2`，约 8 秒后为 `CM,3,3`。

## 未处理事项

- `CM,3,3` 只能证明 MCU 已执行两路命令发送函数，不能证明电机物理接收或执行；若状态到 3 仍不转，应直接用 TTL/示波器测 PB6、PA23 是否出现 13 字节位置命令。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- Tool: `functions.exec`，命令 `C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
