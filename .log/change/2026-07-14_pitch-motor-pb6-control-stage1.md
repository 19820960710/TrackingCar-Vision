# 2026-07-14 Pitch 电机 PB6 控制第一阶段

## 修改目标

- 放弃 PA23/UART2；仅使用 PB6/PB7 的 UART1 控制 Pitch 轴步进电机，并建立供后续视觉追踪调用的相对位置控制接口。

## 修改内容

1. 从 SysConfig 移除 UART2/PA23-PA24 和 UART0/PA0-PA1，仅保留 UART1/PB6-PB7。
   - `stepMotor2_INST` 映射为 UART1；TX 为 PB6，RX 为 PB7；115200、8N1、MFCLK、FIFO。
2. 新增 `pitch_motor_control.*`。
   - `pitch_motor_enable()` 使用代码整理的 `StepperMotor_enable()` 发送使能。
   - `pitch_motor_move_relative()` 将正负脉冲转换为 ZDT CW/CCW 相对实时位置模式（模式 2）命令；速度上限为封装声明的 3000 RPM。
   - `pitch_motor_poll()` 保存电机四字节响应状态。
3. 重写 `main.c`。
   - 只初始化和使能 PB6/PB7 上的 Pitch 电机。
   - 自检开关默认 `0`，不会自动转动；开启后会以 20 脉冲、20 RPM、加速度 10 做正反小角度测试。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.h`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\keil\empty_LP_MSPM0G3507_nortos_keil.uvprojx`

## 验证情况

- SysConfig 1.21.0 重新生成成功，除 TI Flash 状态位提示外无错误。
- TI Arm Clang 已编译 `main.c`、`pitch_motor_control.c`、`zdt_x42s.c`、`stepper_motor.c`、`ti_msp_dl_config.c`，返回码均为 0。
- 待实机验证：电机 RXD 接 PB6、若需响应则电机 TXD 接 PB7、两板共地；启动后先确认使能，不开启自检前不应产生位置移动。

## 未处理事项

- 未接入视觉代码；下一阶段需用视觉横向误差产生有死区、限幅、限频的 `pitch_motor_move_relative()` 请求。
- 自检方向与机械正方向未标定；开启前必须确认行程安全。
- 不为 X42S 添加外部位置 PID；位置环由电机编码器和内部控制器闭合。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\代码整理\drivers\stepper_motor\zdt_x42s\stepper_motor.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- Tool: `functions.exec`，SysConfig CLI、TI Arm Clang，cwd `C:\Users\Aupassen\Desktop\视觉`
