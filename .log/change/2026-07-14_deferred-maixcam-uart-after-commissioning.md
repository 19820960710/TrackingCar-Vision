# 2026-07-14 自检完成后启用 MaixCAM 接收

## 修改目标

- 避免 MaixCAM 上电后的连续 UART 数据在双电机开机自检期间影响主循环；在两路自检完成后再启用 PA1 接收。

## 修改内容

1. 主程序不再在启动阶段立即调用 `vision_uart_init()`。
2. PB6、PA23 两路非阻塞自检完成后，调用 `vision_uart_init()` 并将 `g_vision_uart_ready` 置为真。
3. 在 `vision_uart_init()` 中先关闭 UART0 NVIC 和 RX 中断，清空硬件 RX FIFO 的启动残留数据，再重置软件接收状态并开启 RX、RX timeout 中断和 NVIC。
4. 自检期间不调用视觉接收、解析、回传逻辑；完成后从 PA0 发送一次 `VISION_READY`，随后继续接收 AIM/TV 帧并按既有逻辑回传 `TV,...`。
5. 删除临时 VU/CM 周期诊断，避免它干扰串口观察。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_uart.c`

## 验证情况

- 使用 TI Arm Clang 编译主程序、生成配置、步进电机和视觉模块源文件，编译通过。
- 未实机验证。预期上电后两轴约 7.5 秒完成并发正反自检，TTL 随后收到一次 `VISION_READY`；MaixCAM 继续发送时，成功目标帧会触发 `TV,...` 回传。

## 未处理事项

- 本次刻意丢弃相机启动到自检结束期间的数据；相机持续发送，因此不影响后续跟踪。
- 若 `VISION_READY` 后仍出现接收异常，再单独检查 UART0 接收中断与 MaixCAM 实际输出，不与电机自检耦合排查。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_uart.c`
- Tool: `functions.exec`，命令 `C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
