# 2026-07-13 MSPM0 视觉协议适配层

## 修改目标

- 将当前 MaixCAM 的 `TV` 和 `AIM` 文本行转换为统一的 `vision_observation_t`，使后续云台追踪不依赖视觉算法或激光点格式。

## 修改内容

1. 新增 `vision_config.h`。
   - 集中管理行缓存大小、图像尺寸、协议前缀和数值字段数量。
2. 重写 `vision_packet.c/.h`。
   - 支持 `TV` 与 `AIM` 完整行解析、字段范围校验、协议类型识别和观测转换。
   - AIM 目标有效性由 `target_mode` 判断，允许有效目标位于 `(0, 0)`。
3. 重写 `vision_uart.c/.h`。
   - 用 `vision_uart_config_t` 在应用层绑定实际 UART，取消 UART0 硬编码。
   - 对外只发布 `vision_observation_t`，并将行缓存溢出和解析失败分开计数。
   - 在 ISR/主循环交接行缓存时保存原 PRIMASK；主循环观测状态不使用无效关中断保护。
4. 更新模块 README 和新增协议黑盒测试源。

## 涉及文件

- `mspm0/vision_comm/vision_config.h`
- `mspm0/vision_comm/vision_packet.c`
- `mspm0/vision_comm/vision_packet.h`
- `mspm0/vision_comm/vision_uart.c`
- `mspm0/vision_comm/vision_uart.h`
- `mspm0/vision_comm/tests/vision_packet_test.c`
- `mspm0/vision_comm/README.md`

## 验证情况

- 已使用 TI Arm Clang 4.0.2.LTS 以 `-std=c11 -Wall -Wextra -Werror` 分别编译
  `vision_packet.c` 与 `tests/vision_packet_test.c`，无编译诊断。
- 已执行 `git diff --check`，无空白错误。
- 当前仓库未包含 MSPM0 的生成文件 `ti_msp_dl_config.h`，故未在此工作区编译
  `vision_uart.c`；需要合入实际 Keil/SysConfig 工程后编译。
- ARM 交叉编译生成的测试二进制不能在本机直接运行，`assert` 用例尚未实际执行。
- 未进行真机验证。硬件前提为 MaixCAM 使用 UART4/MCU UART3、115200 baud，且与 MSPM0 共地。

## 未处理事项

- 接收端仍是单行缓存。第二步仅先分离 `overrun_count`，下一步改为环形缓冲并做高频串口压力测试。
- 未实现观测超时和云台控制；这些属于后续跟踪控制步骤。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\log\review_vision_comm.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\maixcam\drivers\uart_output.py`
- Tool: `functions.apply_patch`, cwd `C:\Users\Aupassen\Desktop\视觉`
- Tool: `functions.shell_command`, command `tiarmclang.exe -std=c11 -Wall -Wextra -Werror ... -c`, cwd `C:\Users\Aupassen\Desktop\视觉`
