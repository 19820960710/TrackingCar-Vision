# 2026-07-17 执行机构组合验证

## 修改目标

- 增加左轮+yaw、双轮+yaw+pitch 的两阶段组合动作验证。
- 将原单步进电机服务扩展为 UART0 总线上的 yaw/pitch 两轴服务，同时保留旧单轴接口。

## 修改内容

1. 步进服务增加轴枚举、双实例、独立状态队列和按响应地址路由。
2. 应用层增加显式 yaw/pitch 轴控制接口；原接口继续默认控制 yaw。
3. 新增一次性验证任务和集中参数配置，上电延迟 3 秒后执行低速有限行程动作，结束后停车并失能步进轴。
4. 验证模式启用时不创建 yaw 按键任务，避免它在实验期间改写轮速目标。
5. Keil 工程加入新增 app/config 文件。

## 涉及文件

- `Component/app/actuator_validation.c`
- `Component/app/actuator_validation.h`
- `Component/config/actuator_validation_config.h`
- `Component/config/uart0_role_config.h`
- `Component/service/stepper_service.c`
- `Component/service/stepper_service.h`
- `Component/task/app_tasks.c`
- `Component/task/app_tasks.h`
- `Component/zdt_x42s/zdt_x42s.c`
- `Component/zdt_x42s/zdt_x42s.h`
- `Component/zdt_x42s/stepper_motor.c`
- `Component/zdt_x42s/stepper_motor.h`
- `docs/执行机构组合验证说明.md`
- `keil/M0_Templant_FreeRTOS.uvprojx`

## 验证情况

- 使用 Keil Arm Compiler 6.24 构建，结果为 `0 Error(s), 0 Warning(s)`。
- 当前未烧录。用户确认两个轴使用独立线路且地址均为 `0x01`；第二路 UART 引脚尚未确认，因此验证开关暂时关闭。

## 未处理事项

- 未实现 ZDT 多轴硬件同步触发；当前两个位置帧由同一 UART 依次发送，启动相差若干毫秒。
- 未执行实机动作和响应数据采集。
- 尚未把 pitch 轴绑定到第二路 UART，需根据实际接线补充 SysConfig 和 UART 适配层。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\代码整理\features\zdt_x42s_stepper\README.md`
- Source: `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\service\stepper_service.c`
- Source: `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\Component\service\speed_service.h`
- Tool: `functions.shell_command`，command `UV4.exe -b M0_Templant_FreeRTOS.uvprojx -j0`，cwd `C:\Users\Aupassen\Desktop\功能实验和验证\M0_Templant_FreeRTOS\keil`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Documents\25年E题`
