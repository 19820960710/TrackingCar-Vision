# 2026-07-14 PB22 板载 LED 烧录验证

## 修改目标

- 增加 PB22 板载 LED 心跳，用于判断当前编译的固件是否实际烧录并运行在开发板上。

## 修改内容

1. 在 `main.c` 增加 `led_heartbeat_update()`。
2. 使用已由 SysConfig 配置的 PB22 输出，每 500 ms 翻转一次。
3. 保留 PA23 每 1000 ms 翻转测试，不恢复 UART2 复用。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 使用 TI Arm Clang 编译主程序、生成配置、步进电机和视觉模块源文件，编译通过。
- 未实机烧录验证。烧录后预期 PB22 板载 LED 每 0.5 秒改变一次亮灭状态。

## 未处理事项

- LED 的电气有效电平由开发板硬件决定，但使用翻转操作时，无论有效高或有效低，肉眼均应看到亮灭变化。
- 若 LED 不闪，应先确认 Keil 的下载算法、目标芯片和实际烧录的输出文件；此时不能根据 PA23 电压判断 UART 或硬件网络。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Tool: `functions.exec`，命令 `C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
