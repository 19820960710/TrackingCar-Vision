# 修改目标

把已在本地 Keil 工程验证通过的 MSPM0G3507 视觉串口通信模块整理进 GitHub 仓库，方便后续 MaixCAM Pro 与主控联调复用。

# 修改内容

- 新增 `mspm0/vision_comm/vision_uart.c/.h`，负责 UART0 中断接收、按行缓存、调用解析器并保存最新视觉包。
- 新增 `mspm0/vision_comm/vision_packet.c/.h`，负责解析 MaixCAM Pro 的 `AIM,...` 文本协议。
- 新增 `mspm0/vision_comm/README.md`，记录接线、SysConfig 要求、Keil 接入代码和读取接口。
- 更新 `docs/uart_protocol.md`，补充 MSPM0G3507 接收模块路径和默认接线。

# 涉及文件

- `mspm0/vision_comm/vision_uart.c`
- `mspm0/vision_comm/vision_uart.h`
- `mspm0/vision_comm/vision_packet.c`
- `mspm0/vision_comm/vision_packet.h`
- `mspm0/vision_comm/README.md`
- `docs/uart_protocol.md`

# 验证情况

- 用户已在 Keil 中确认本地工程版本编译通过。
- 仓库中保留可复用源码和接入说明，未上传完整 Keil 临时工程及 SDK 复制文件。

# 未处理事项

- 仓库内模块仍需在后续云台联调工程中按 `README.md` 加入 Keil 工程并实机通信验证。

# 依据与工具

- 依据当前 MaixCAM Pro UART 输出协议 `AIM,valid,dx,dy,target_x,target_y,laser_x,laser_y,...`。
- 依据本地 MSPM0G3507 Keil 工程编译通过版本整理。
