# 2026-07-14 云台 UART 周期发送路径审查

## 检查范围
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\gimbal_motor.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\gimbal_motor.h`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.h`
- `C:\Users\Aupassen\Desktop\代码整理\drivers\stepper_motor\zdt_x42s\zdt_x42s.c`

## 检查依据
- 实机现象：将“代码整理”的步进电机封装置于循环调用后，串口助手可收到周期性信号；当前工程烧录后收不到。
- 两份封装均使用 `DL_UART_Main_isTXFIFOFull()` 等待和 `DL_UART_Main_transmitData()` 写发送 FIFO。

## 发现

1. 当前周期诊断的 UART 实例与 Yaw 配置不一致，必须修复。
   - 影响：串口助手接在 Yaw 的 PA23 时，不会看到 `main.c` 中诊断函数发送的字节。
   - 证据：`main.c:53-57` 将 Yaw 配置为 `stepMotor1_INST`；生成头文件 `ti_msp_dl_config.h:82-93` 映射该实例到 UART2/PA23。`main.c:94` 和 `main.c:96` 却固定使用 `stepMotor2_INST`；生成头文件 `ti_msp_dl_config.h:98-109` 映射它到 UART1/PB6。
   - 处理：采纳。后续仅将周期测试改为通过 `g_yaw_motor` 调用封装，以 PA23/UART2 为唯一观测口。

2. 当前封装的查询函数没有调用点，必须修复。
   - 影响：`gimbal_motor_request_speed()` 本身不会执行，不能以它验证封装发送是否正常。
   - 证据：`gimbal_motor.c:78-91` 定义查询帧发送；工程内唯一的主循环调用是 `main.c:197` 的 `gimbal_uart_diagnostic_update()`，该函数 `main.c:81-103` 直接写 UART FIFO，没有调用 `gimbal_motor_request_speed()`。运动自检在 `main.c:25` 被编译开关设为 0，因此也不会调用封装发送函数。
   - 处理：采纳。后续将周期调用改为 `gimbal_motor_request_speed(&g_yaw_motor)`，不再保留同功能的裸 FIFO 发送。

3. UART2/UART1 的生成初始化存在，当前证据不支持“未初始化”的判断。
   - 影响：不应在没有证据的情况下重写 SysConfig 或手工添加第二套 UART 初始化，以免遮蔽真正的调用路径问题。
   - 证据：`ti_msp_dl_config.c:49-57` 的 `SYSCFG_DL_init()` 依次调用 `SYSCFG_DL_stepMotor1_init()`、`SYSCFG_DL_stepMotor2_init()`；`ti_msp_dl_config.c:175-195` 初始化 UART2，设置 MFCLK、115200、8N1、FIFO 并 `DL_UART_Main_enable(stepMotor1_INST)`；`ti_msp_dl_config.c:210-229` 对 UART1 做相同处理。`ti_msp_dl_config.c:105-114` 已将 PA23 和 PB6 配置为外设 TX 功能。`main.c:186` 在任何发送前调用 `SYSCFG_DL_init()`。
   - 处理：暂不采纳“新增 UART 初始化”。需要实机验证修改后的 UART2/PA23 周期帧；若仍无信号，再读取 UART2 寄存器和检查下载的 HEX，而不是叠加初始化代码。

4. 当前封装的逐字节发送实现与“代码整理”一致，不是本次差异。
   - 影响：没有必要替换发送循环。
   - 证据：当前 `gimbal_motor.c:22-29` 和对照 `zdt_x42s.c:11-25` 都是在 FIFO 未满时逐字节调用 `DL_UART_Main_transmitData()`。
   - 处理：暂不修改。

## 已采纳
- 以 UART2/PA23（Yaw）作为下一次唯一的串口观测目标。
- 将周期发送切换到已初始化的 `g_yaw_motor` 封装接口，删除其重复的裸 FIFO 发送路径。

## 未采纳
- 不额外手工初始化 UART2/UART1：生成初始化和主程序调用链均存在；当前无证据显示初始化未执行。
- 不修改 TX 上拉/下拉：PA23 已配置为 UART2 TX 外设输出，TX 空闲电平由 UART 外设驱动，内部下拉会制造错误空闲状态。

## 验证情况

- 静态核对完成：工程文件 `keil\empty_LP_MSPM0G3507_nortos_keil.uvprojx` 已包含 `main.c`、`gimbal_motor.c` 和 `ti_msp_dl_config.c`。
- 未做实机验证：下一次最小修改后，需以 USB-TTL 的 RXD 接 PA23、GND 接开发板 GND，115200/8N1/HEX 观察每秒一次的 `01 35 6B`。

## 依据与工具
- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\gimbal_motor.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\ti_msp_dl_config.c`
- Source: `C:\Users\Aupassen\Desktop\代码整理\drivers\stepper_motor\zdt_x42s\zdt_x42s.c`
- Tool: `functions.exec`，PowerShell `Get-Content`、`Select-String`，cwd `C:\Users\Aupassen\Desktop\视觉`
