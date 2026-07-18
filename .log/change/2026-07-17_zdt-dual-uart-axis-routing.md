# 2026-07-17 ZDT 双 UART 轴路由

## 修改目标

- 根据实物接线把 yaw 与 pitch 从“同 UART 不同地址”改为“两路 UART、相同地址”。

## 修改内容

1. SysConfig 新增并固定两路串口：yaw=UART2 PA23/PA24，pitch=UART1 PB6/PB7，均为 115200 8N1。
2. 新增 `stepper_uart`，每个轴独立拥有 TX 互斥锁、RX 队列和中断入口。
3. `stepper_service` 按轴选择 UART transport，两个 ZDT 协议实例地址均为 `0x01`。
4. 删除会造成命名误导的 `uart0.*` 和 `uart0_role_config.h`。
5. 更新工程说明并重新开启组合验证任务。

## 涉及文件

- `main.syscfg`
- `ti_msp_dl_config.c`
- `ti_msp_dl_config.h`
- `Component/UART/stepper_uart.c`
- `Component/UART/stepper_uart.h`
- `Component/config/stepper_uart_config.h`
- `Component/service/stepper_service.c`
- `Component/config/actuator_validation_config.h`
- `main.c`
- `keil/M0_Templant_FreeRTOS.uvprojx`
- `docs/ZDT_X42S_FreeRTOS迁移说明.md`
- `docs/执行机构组合验证说明.md`

## 验证情况

- SysConfig 1.26.2 成功生成 UART2 PA23/PA24 和 UART1 PB6/PB7，无引脚冲突。
- Arm Compiler 6.24 在重新开启组合验证任务后构建结果为 `0 Error(s), 0 Warning(s)`，程序大小 `Code=45832, RO-data=13520, RW-data=20, ZI-data=28364`。
- `git diff --check` 通过。

## 未处理事项

- 尚未烧录和执行实机动作；方向、机械行程和响应数据仍需实测。
- 两个位置帧由同一 FreeRTOS 服务任务依次发送，不是驱动器硬同步触发。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\plugins\cache\openai-primary-runtime\pdf\26.715.12143\skills\pdf\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\功能实验和验证\SCH_Schematic1_2026-06-13.pdf`
- Source: `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\main.syscfg`
- Tool: `functions.shell_command`，command `SysConfig CLI --compiler keil main.syscfg`，cwd `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS`
- Tool: `functions.shell_command`，command `UV4.exe -b M0_Templant_FreeRTOS.uvprojx -j0`，cwd `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\keil`
