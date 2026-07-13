# 2026-07-13 MSPM0 视觉接收环形缓冲与时效判断

## 修改目标

- 防止主循环短暂阻塞时丢弃后续所有视觉帧，并为云台控制层提供统一的观测超时判断。

## 修改内容

1. 将 UART ISR 的单行缓存替换为 256 字节单生产者/单消费者环形缓冲。
   - ISR 只写入字节，主循环负责换行拼帧与协议解析。
   - 满缓冲时丢弃新字节并累计 `overrun_count`；主循环在下一换行符前丢弃数据以重新同步。
   - 一次处理多个有效帧时只保留最新观测，符合追踪控制的低延迟需求。
2. 新增 `vision_observation_is_fresh()`。
   - 以无符号毫秒差判断时效，支持 32 位毫秒计数回绕。
   - 云台控制层应在观测超时后保持电机位置并清零控制输出；本模块不直接控制电机。
3. 采纳复审中的 TV `mode` 校验、分派收紧、同上下文注释、接线文档与测试扩展。

## 涉及文件

- `mspm0/vision_comm/vision_config.h`
- `mspm0/vision_comm/vision_uart.c`
- `mspm0/vision_comm/vision_uart.h`
- `mspm0/vision_comm/vision_observation.c`
- `mspm0/vision_comm/vision_observation.h`
- `mspm0/vision_comm/vision_packet.c`
- `mspm0/vision_comm/tests/vision_packet_test.c`
- `mspm0/vision_comm/README.md`
- `docs/uart_protocol.md`

## 验证情况

- 使用 TI Arm Clang 4.0.2.LTS、`-std=c11 -Wall -Wextra -Werror` 编译观测层、协议层和测试源，无诊断。
- 使用最小 TI API stub 编译 UART 驱动，无诊断；实际 `ti_msp_dl_config.h` 仍需在 Keil 工程中验证。
- 已执行 `git diff --check`，无空白错误。
- 未执行 ARM 交叉编译出的断言测试，未在 MaixCAM/MSPM0 真机测试环形缓冲。

## 未处理事项

- 需在实际 Keil/SysConfig 工程绑定板载 UART4 对应 UART3 并进行整工程编译。
- 需通过真机记录 `overrun_count`、视觉帧率和端到端延迟后确定控制层超时阈值；README 的 100 ms 仅为集成示例。
- 尚未实现云台外环、步进电机命令和机械限位保护。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\.log\review\2026-07-13_vision-comm-c-style-review.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\maixcam\drivers\uart_output.py`
- Tool: `functions.apply_patch`, cwd `C:\Users\Aupassen\Desktop\视觉`
- Tool: `functions.shell_command`, command `tiarmclang.exe ... -c`, cwd `C:\Users\Aupassen\Desktop\视觉`
