# 2026-07-13 vision_comm 复审意见处置

## 检查范围

- `.log/review/2026-07-13_vision-comm-c-style-review.md` 的新增问题 2.1 至 2.8。

## 检查依据

- `maixcam/drivers/uart_output.py` 中 TV/AIM 的实际生成逻辑。
- UART 接收驱动保持板级类型安全、协议层可独立测试的模块边界。

## 发现

1. TV 模式的 `valid=1` 与 `mode=LOST` 若发生矛盾，旧代码会继续发布目标。
   - 影响: 云台可能跟踪已丢失目标。
   - 处理: 采纳。TV 与 AIM 一样拒绝将 `LOST`/`NO_TARGET` 发布为有效目标。
2. 协议分派仅检查首字符。
   - 影响: 当前不会误解析，但新增其他 A/T 开头协议时分派路径不够明确。
   - 处理: 采纳。改为检查 `AI` 和 `TV` 前两字符，完整前缀仍由解析器验证。
3. UART 头文件依赖 TI 生成头文件。
   - 影响: UART 驱动不能脱离 TI SDK 在宿主机直接编译。
   - 处理: 暂不采纳。该文件是 MSPM0 板级驱动，`UART_Regs *` 与 `IRQn_Type` 的类型检查比用 `void *` 更重要；协议与观测层已能独立编译测试。
4. 主循环观测读写的上下文约束未写完整。
   - 影响: 后续移入 ISR 或不同 RTOS 任务时可能发生非原子结构体读写。
   - 处理: 采纳。在 `vision_uart_take_latest_observation()` 注释中明确要求与 `vision_uart_process()` 位于同一执行上下文。
5. 文档仍含 UART0/PA10/PA11 接线。
   - 影响: 可能导致 MaixCAM 接到与云台电机冲突的串口。
   - 处理: 采纳。同步为板载 UART4 / MCU UART3 / PB2 PB3。
6. 兼容 AIM-only 解析接口未使用，且 packet 内部状态可进一步改名。
   - 影响: 当前无功能错误。
   - 处理: 暂不改动。公共接口尚未进入实际工程，不在环形缓冲改造中叠加无关 API 变更；`aim_valid` 已有协议层注释。
7. 协议边界测试不足。
   - 影响: 异常状态或毫秒计数回绕缺少回归检查。
   - 处理: 采纳。补充 AIM 状态矛盾、TV LOST、空行、数值溢出和时间回绕断言。

## 已采纳

- TV mode 防御性校验、双字符协议分派、同上下文说明、接线文档和测试扩展。

## 未采纳

- UART 配置类型抽象为 `void *`/整数：保留 TI 类型安全。
- AIM-only 兼容接口删除与内部命名清理：推迟到接口实际接入工程并确认调用者后。

## 验证情况

- `vision_observation.c`、`vision_packet.c`、`tests/vision_packet_test.c` 已通过 TI Arm Clang 4.0.2.LTS 的 `-std=c11 -Wall -Wextra -Werror` 编译。
- 使用最小 TI API stub 对 `vision_uart.c` 做同样的严格语法编译；stub 不验证实际寄存器或中断行为。
- 未执行 ARM 断言二进制，未进行真机 UART 测试。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\.log\review\2026-07-13_vision-comm-c-style-review.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\maixcam\drivers\uart_output.py`
- Tool: `functions.apply_patch`, cwd `C:\Users\Aupassen\Desktop\视觉`
- Tool: `functions.shell_command`, command `tiarmclang.exe ... -c`, cwd `C:\Users\Aupassen\Desktop\视觉`
