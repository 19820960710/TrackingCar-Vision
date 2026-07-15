# 2026-07-15 目标居中或丢失时立即停止电机

## 修改目标

- 防止实时相对位置命令在目标进入中心或丢失后仍继续执行，降低越过目标并丢失目标的风险。

## 修改内容

1. 在 X42S 协议层加入非阻塞立即停止帧：`地址 FE 98 00 6B`。
2. 在步进电机驱动层和云台电机封装层增加停止接口。
3. 为每个跟踪控制器增加独立的 `motion_command_active` 状态。
4. 当目标无效或误差进入死区时，每个轴只提交一次立即停止；若 UART 正忙，则后续视觉帧继续重试，直至停止帧成功入队。
5. 未修改方向、`0.2 脉冲/像素` 比例、速度、加速度、控制周期和开机自检。

## 涉及文件

- `zdt_x42s.c`
- `zdt_x42s.h`
- `stepper_motor.c`
- `stepper_motor.h`
- `pitch_motor_control.c`
- `pitch_motor_control.h`
- `pitch_tracker_control.c`
- `pitch_tracker_control.h`
- `.log/change/2026-07-15_stop-motor-on-center-or-target-loss.md`

## 验证情况

- Keil 全量编译结果为 `0 Error(s), 0 Warning(s)`。
- XDS110 烧录结果为 `Erase Done`、`Programming Done`、`Verify OK`。
- 需要实机验证目标进入中心或丢失时两个轴是否及时停止，以及停止响应是否被协议解析接受。

## 未处理事项

- 尚未增加“摄像头数据流完全中断”的超时停止；当前停止依赖收到 `target_valid=false` 或中心误差帧。
- 尚未加入速度型 PID，本次只修复停止边界。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\电赛\14.例程_TI_MSPM0G3507\Emm固件模式\串口通讯\MSPM0G3507_串口通讯__位置模式\1\bsp\Emm_V5.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\zdt_x42s.c`
- Tool: `functions.shell_command`，协议与控制逻辑核对，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: Keil UV4 全量编译与烧录，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
