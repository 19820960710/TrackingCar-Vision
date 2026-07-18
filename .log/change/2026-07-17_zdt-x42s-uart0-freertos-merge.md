# ZDT X42S 与 UART0 FreeRTOS 合并

## 目标

将代码整理中的 ZDT X42S 位置控制协议合入当前 FreeRTOS 工程，并判断现有
UART0 是否可复用。

## 结论与实施

- 复用 UART0 的 SysConfig、TX 互斥锁、RX 中断和 FreeRTOS 字节队列。
- ZDT 协议改为 transport 回调，不再直接访问 `UART_Regs` 或 RX FIFO。
- 新增 stepper service 命令队列和后台响应轮询任务。
- 新增应用层使能、移动、状态查询接口。
- UART0 当前独占给步进电机；关闭启动文本、printf、心跳和 echo。
- UART0 切回调试角色时，stepper service 自动停用且不阻塞其他任务启动。
- 初始地址为 0x01，115200 8N1；模块上电不自动运动。

## 验证边界

- Arm Compiler 6.24 全量构建通过：0 error，0 warning。
- 尚未烧录，地址、电平、脉冲/圈和 Emm 固件响应格式需要实物确认。

## 使用的技能与来源

- `c-style`：分离 UART transport、设备协议、RTOS service 与 app 接口。
- `project-logbook`：记录复用结论、危险边界和未完成的实机验证。
- 来源：`C:\Users\Aupassen\Desktop\代码整理\features\zdt_x42s_stepper`。
