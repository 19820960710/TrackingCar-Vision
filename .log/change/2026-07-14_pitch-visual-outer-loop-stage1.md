# 2026-07-14 Pitch 视觉外环第一阶段

## 修改目标

- 仅使用 PB6/PB7 的 UART1 控制 Pitch 轴，并建立供视觉误差调用的外环控制接口。

## 修改内容

1. 从 SysConfig 移除 UART2/PA23-PA24。
   - 生成配置仅保留 UART1：PB6 TX、PB7 RX、115200、8N1、MFCLK、FIFO。
2. 主程序只初始化和使能 PB6/PB7 上的 Pitch 电机。
   - 删除 PA23 对应电机上下文和重复使能发送。
3. 新增 `pitch_tracker_control.*`。
   - `pitch_tracker_update()` 输入目标有效标志、目标相对画面中心的纵向像素误差和时间戳。
   - 控制器使用死区、比例像素到脉冲换算、单次脉冲限幅和 40 ms 发送限频，调用 `pitch_motor_move_relative()`。
   - 视觉误差正方向与电机 CW 的关系通过 `positive_error_is_cw` 显式保留，需实机标定。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.h`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\keil\empty_LP_MSPM0G3507_nortos_keil.uvprojx`

## 验证情况

- SysConfig 1.21.0 重新生成成功，除 TI Flash 状态位提示外无错误。
- TI Arm Clang 已编译 `main.c`、`pitch_motor_control.c`、`pitch_tracker_control.c`、`zdt_x42s.c`、`stepper_motor.c`、`ti_msp_dl_config.c`，返回码均为 0。
- 待实机验证：PB6 接电机 RXD、PB7 接电机 TXD（若读取响应）、共地；当前只发送使能，不自动发送位置运动。

## 未处理事项

- 当前没有接入 MaixCAM 的 `target_valid`、`error_y_pixels` 与 `now_ms`；后续视觉模块需调用 `pitch_tracker_update()`。
- 未开启自动小角度测试，需先确认 Pitch 的正方向和机械行程。
- 默认 `0.25` 脉冲/像素、8 像素死区、30 脉冲限幅、40 ms 周期均为初始保守值，需要实机标定。
- 不增加外部电机位置 PID；X42S 内部编码器负责位置闭环，主控外环仅根据视觉误差给增量指令。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- Tool: `functions.exec`，SysConfig CLI、TI Arm Clang，cwd `C:\Users\Aupassen\Desktop\视觉`
