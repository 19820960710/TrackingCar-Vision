# 2026-07-15 电脑UART视觉仿真与电机应答诊断

## 修改目标

- 在不连接MaixCAM的情况下，以接近实际AIM文本线速的频率向MSPM0发送视觉目标。
- 同时记录双轴运动命令提交、UART发送和X42S应答结果，用于定位双轴互相影响发生在哪一层。

## 修改内容

1. 新增电脑端UART仿真脚本。
   - 默认115200 bit/s、250 Hz，约4 ms发送一帧AIM文本。
   - 循环产生yaw单轴、pitch单轴、双轴正负误差和回中场景。
   - 同一场景内连续发送视觉帧，保留对UART0中断、协议解析和200 ms电机限速的压力。
   - 同时读取并可保存主控TV/MS回传。
2. 恢复双轴跟踪编译开关。
   - yaw与pitch均为`1U`。
3. 扩展TV输出。
   - 增加`CS,pitch_submit,yaw_submit`，表示该视觉帧是否成功提交对应轴运动命令。
4. 增加每轴诊断统计。
   - 统计运动命令排队/拒绝、协议帧排队/完成/忙拒绝/超时。
   - 统计电机返回`02`、`9F`、`E2/EE`和协议错误。
   - 修正轮询无新帧时覆盖`last_response`的问题。
   - 每500 ms尝试输出一条轴状态，yaw和pitch交替输出。
5. 选择电脑UART重放而非MCU内部观测仿真。
   - 原因是需要同时覆盖UART0接收中断和视觉文本解析负载；内部直接构造观测会绕过该路径。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\zdt_x42s.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\zdt_x42s.h`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.h`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\maixcam_uart_simulator.py`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\README_maixcam_uart_simulator.md`

## 验证情况

- 使用TI ARM Clang编译main、电机、视觉通信和生成配置源文件，退出码为0。
- 使用Python AST解析仿真脚本，语法检查通过。
- 尚未进行真实串口和电机应答验证。
- 默认250 Hz来源于当前AIM文本约46字节、115200 8N1下约4 ms的线速时间；实际摄像头帧率若不同，可通过`--fps`调整。

## 未处理事项

- 电机返回帧解析器仍采用固定4字节收帧，若实测出现协议错误持续增长，需要增加逐字节重同步状态机。
- 当前尚未加入等待`02/9F`后再下发下一命令的调度器；本次先收集证据。
- 当前视觉UART ISR和主循环软件环形缓冲仍会一次排空现有数据；若LED/TV/MS一起停止，再增加每轮处理预算。
- 需要确认X42S已配置为发送应答，并连接pitch TXD到PB7、yaw TXD到PA24。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\zdt_x42s.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_uart.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_observation.h`
- Source: `C:\Users\Aupassen\Desktop\代码整理\drivers\stepper_motor\zdt_x42s\zdt_x42s.c`
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.shell_command`，command `tiarmclang.exe ... -c <application sources>`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
- Tool: `functions.shell_command`，command `python -c ast.parse(...)`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
