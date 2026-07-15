# 2026-07-14 双电机上电自检

## 修改目标

- 使 PB6 和 PA23 两个步进电机通讯通道均在上电后执行正反向自检。

## 修改内容

1. 新增 `stepMotor1`：UART1、PB6 为 TX、PB7 为 RX、115200 baud。
2. 保留 `stepMotor2`：UART2、PA23 为 TX、PA24 为 RX、115200 baud。
3. 新增 `g_pb6_motor` 实例，与既有 `g_pitch_motor` 分别初始化、使能和轮询。
4. 保持 `PITCH_MOTOR_COMMISSIONING_TEST_ENABLED = 1U`，上电时顺序执行：先 PB6 通道，后 PA23 通道；每个通道正向 1600 脉冲、等待、反向 1600 脉冲。顺序执行避免两个机械轴同时运动。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`

## 验证情况

- SysConfig 生成成功：`stepMotor1_INST = UART1`（PB6/PB7），`stepMotor2_INST = UART2`（PA23/PA24）。
- 使用 TI Arm Clang 编译主程序、生成配置、步进电机和视觉模块源文件，编译通过。
- 未进行实机运动验证。烧录前应确认两个轴均无机械干涉并留有足够行程。

## 未处理事项

- `g_pitch_tracker` 仍只连接 `g_pitch_motor`（PA23 通道）；PB6 通道当前仅用于上电自检和通讯验证。
- 自检为阻塞式延时；上电后须等待两个通道自检完成，主循环和视觉处理才会开始。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- Tool: `functions.exec`，命令 `C:\ti\sysconfig_1.21.0\sysconfig_cli.bat ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.exec`，命令 `C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
