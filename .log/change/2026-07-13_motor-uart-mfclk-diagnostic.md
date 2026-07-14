# 2026-07-13 电机 UART MFCLK 校准与响应轮询

## 修改目标

- 针对 UART2 PA23 未观测到诊断帧的问题，使电机 UART 时钟和发送方式与厂家 X42S 串口例程一致，并为后续读取电机确认帧准备非阻塞接口。

## 修改内容

1. 将 `stepMotor1`（UART2）和 `stepMotor2`（UART1）的 UART 时钟源改为 MFCLK。
   - 在 `empty.syscfg` 开启 `MFCLKGATE` 与时钟树配置。
   - 生成代码使用 `DL_UART_MAIN_CLOCK_MFCLK` 和 4 MHz MFCLK 的 115200 波特率分频；引脚保持 Yaw PA23/PA24、Pitch PB6/PB7 不变。
2. 保持原来的 `DL_UART_Main_transmitDataBlocking()` 发送 API、诊断帧和主循环不变。
   - 根据用户审查意见，本轮只保留时钟源这一项变量；TX FIFO 直写和响应轮询延后到 MFCLK 实物结果明确后再单独引入。
3. 保持每秒发送 `01 35 6B` 的无运动转速查询诊断；电机自检仍关闭。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\gimbal_motor.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\gimbal_motor.h`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- SysConfig 1.21.0 生成成功；生成文件包含 `DL_SYSCTL_enableMFCLK()`、`DL_UART_MAIN_CLOCK_MFCLK` 与 `stepMotor[12]_IBRD_4_MHZ_115200_BAUD`。
- 尚未完成实物验证。需重新 Build、Download、RESET 后，以 115200 bit/s 监听 PA23，预期每秒一帧 `01 35 6B`。
- 仅接 MCU TX 和 GND 时可以验证发送；接入 PA24 与电机 TX 后才可验证响应轮询。

## 未处理事项

- 尚未恢复运动自检，等待确认 PA23 诊断帧可见后再开启。
- 电机响应轮询未接入，等待先完成单变量 MFCLK 验证；届时需要连接 PA24 与电机 TX。
- SYSCTL 生成时给出 32 MHz 下 Flash 状态清除的提示；本次程序不执行 Flash 擦写/编程，暂不处理。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\代码整理` commit `253f359e001befc190ee52f3002905934a3423d3` 的 `drivers/stepper_motor/zdt_x42s/zdt_x42s.c`
- Source: `C:\Users\Aupassen\Desktop\电赛\14.例程_TI_MSPM0G3507\Emm固件模式\串口通讯\MSPM0G3507_串口通讯__位置模式\1\ti_msp_dl_config.c`
- Tool: `functions.exec`，命令 `git push origin main`，cwd `C:\Users\Aupassen\Desktop\代码整理`
- Tool: `functions.exec`，命令 `sysconfig_cli.bat ... empty.syscfg`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
