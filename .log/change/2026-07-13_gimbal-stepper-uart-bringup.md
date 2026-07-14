# 2026-07-13 云台步进电机 UART 控制接入

## 修改目标

- 在 Keil 工程中接入两台 X42S 步进电机的 UART 命令驱动，为后续视觉外环跟踪提供 Yaw/Pitch 执行接口。

## 修改内容

1. 更新 SysConfig 串口配置。
   - `stepMotor1` 使用 UART2（PA23/PA24），作为 Yaw。
   - `stepMotor2` 使用 UART1（PB6/PB7），作为 Pitch。
   - `maxicam` 使用 UART3（PB2/PB3）、115200 bit/s，并启用接收及接收超时中断。
2. 在 `main.c` 初始化 Yaw 与 Pitch 的 `gimbal_motor_t`，并将视觉 UART 实例改为 SysConfig 生成的 `maxicam_INST`。
3. 保持默认安全状态：上电只初始化软件对象，不向任意电机发送使能、运动或停止命令。
4. 增加可选的 Yaw 自检状态机。仅在将 `GIMBAL_MOTOR_SELF_TEST_ENABLED` 改为 `1U` 后执行：上电等待、使能、正向 160 脉冲、反向 160 脉冲、停止并失能。16 微步条件下 160 脉冲约为 18 度。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\gimbal_motor.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\gimbal_motor.h`

## 验证情况

- 已使用 SysConfig 1.21.0 生成三路 UART 配置；生成结果包含 `stepMotor1_INST`、`stepMotor2_INST` 和 `maxicam_INST`。
- 已使用 `C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe` 单独编译 `main.c`、`gimbal_motor.c`、视觉通信模块和 `ti_msp_dl_config.c`，退出码为 0。
- 编译输出仅含 TI DriverLib 头文件中已有的 3 处未使用形参警告；本次源文件没有诊断。
- 尚未在实物电机上测试。自检必须先确认限位与机械活动范围，再手动将宏改为 `1U` 后烧录。

## 未处理事项

- 未实现视觉坐标到脉冲的外环控制、限位保护、堵转/离线检测和电机反馈读取；这些属于下一阶段跟踪控制。
- `positive_is_cw` 暂设为 `true`。实测方向与期望相反时，应只修改对应电机配置为 `false`，不要改动底层协议。
- 电机地址均为 1，因为每台电机独占一条 UART；若以后共用总线，必须配置为不同地址。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\电赛\14.例程_TI_MSPM0G3507\Emm固件模式\串口通讯` 中的 X42S UART 示例；`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\gimbal_motor.c`
- Tool: `functions.exec`，命令 `sysconfig_cli.bat ... empty.syscfg`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.exec`，命令 `tiarmclang.exe ... -c`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
