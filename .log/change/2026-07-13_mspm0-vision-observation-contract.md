# 2026-07-13 MSPM0 目标观测契约

## 修改目标

- 为后续云台跟踪建立不依赖 `AIM`、`TV` 或具体视觉算法的统一目标图像观测接口。

## 修改内容

1. 新增 `mspm0/vision_comm/vision_observation.h`。
   - 定义 `vision_observation_t`，统一保存目标有效性、目标像素坐标、图像尺寸、主控接收序号、接收时刻和来源协议。
   - 规定云台控制层只读取该结构，不直接读取 UART 文本或 `AIM` 激光精瞄误差。
2. 更新 `mspm0/vision_comm/README.md`。
   - 说明协议解析层负责把 `AIM` 或 `TV` 转换为统一观测，云台控制层与视觉算法解耦。
3. 采纳 `.log/review/2026-07-13_mspm0-vision-uart-interface-review.md` 中关于“目标观测与激光精瞄语义分离”的审查项。

## 涉及文件

- `mspm0/vision_comm/vision_observation.h`
- `mspm0/vision_comm/README.md`
- `.log/change/2026-07-13_mspm0-vision-observation-contract.md`

## 验证情况

- 检查头文件不包含 `ti_msp_dl_config.h`、UART 寄存器类型或视觉协议字段，控制层可独立包含。
- 当前环境未发现 `gcc` 或 `clang`，未执行宿主机 C 编译；需要在后续 Keil 工程中验证头文件兼容性。
- 未进行真机验证；本步骤不包含 UART 收发或电机动作。

## 未处理事项

- 现有 `vision_packet_t`、`vision_uart_data_t` 尚未转换为 `vision_observation_t`；该改动留给下一步协议与接收层重构。
- 当前 `AIM` 解析器的文本校验、UART 硬编码实例、帧超时和环形缓冲区问题未在本步骤修改，避免同时改变公共数据结构和传输行为。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\vision_comm\vision_packet.h`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\vision_comm\README.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\.log\review\2026-07-13_mspm0-vision-uart-interface-review.md`
- Tool: `functions.apply_patch`，用于新增公共契约、更新接口说明和记录变更日志，cwd `C:\Users\Aupassen\Desktop\视觉`
