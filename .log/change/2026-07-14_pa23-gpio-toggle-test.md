# 2026-07-14 PA23 普通 GPIO 翻转判别测试

## 修改目标

- 将 PA23 临时作为普通数字输出，以区分 PA23 的硬件网络问题与 UART2 复用/高速信号问题。

## 修改内容

1. 在 SysConfig 中增加 `GPIO_GRP_PA23_TEST`，将 PA23 配置为普通数字输出。
2. 在主循环中按 1000 ms 周期翻转 PA23 电平；上电初始化电平为低。
3. 不修改 MaixCAM、PB6/PB7 电机 UART 的配置及其业务逻辑。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- SysConfig 已成功生成配置，PA23 对应 GPIOA.23、IOMUX PINCM53，初始化为数字输出、初始低电平。
- 使用 TI Arm Clang 编译主程序、生成配置、步进电机和视觉模块源文件，编译通过。
- 未进行实机测量；需烧录后在 PA23 排针相对 GND 测量。预期每秒切换一次，完整高低周期为 2 秒。

## 未处理事项

- 本次不恢复 UART2/PA23 的 UART 复用，避免测试结果受 UART 外设影响。
- 若 GPIO 也不翻转，需要检查开发板 PA23 排针到 MCU 的网络、VREF+ 外围连接及板级硬件；若 GPIO 能翻转而 UART 不能，再检查 UART2 复用和该网络的高频负载。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- Tool: `functions.exec`，命令 `C:\ti\sysconfig_1.21.0\sysconfig_cli.bat ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.exec`，命令 `C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
