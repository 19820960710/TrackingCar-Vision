# 2026-07-14 添加 V2 串口协议与 MSPM0 安全状态参考

## 修改目标

- 保持 V2 首版 ASCII AIM 文本协议兼容现有 MSPM0 接收端；独立定义未启用的二进制 V1 帧、CRC、序号、状态位和主控安全决策，供 MSPM0 固件后续同步升级。

## 修改内容

1. 固定 ASCII 首版
   - `UART_PROTOCOL_MODE` 保持 `ascii_aim`，未修改 `AIM,valid,dx,dy,...` 帧格式。
   - 增加配置注释，禁止在 MSPM0 二进制解析验收前切换协议模式。
2. 实现二进制 V1 编解码
   - 固定 22 字节、小端帧：帧头、版本、16 位序号、状态位、`age_ms`、靶心/激光/误差坐标和 CRC-16/IBM。
   - 状态位区分目标有效、激光有效、预测、丢失和过期；编码时目标失效或 `age_ms>100` 自动置丢失/过期状态。
3. 实现 MSPM0 安全策略参考
   - 正常帧 `FOLLOW`；预测帧默认 `HOLD_PREDICTED`；无激光帧 `HOLD_NO_LASER`；丢失或过期帧 `STOP_LOST`。
   - 旧序号、CRC 错、帧头/版本/长度错误帧均丢弃且不刷新看门狗；超过 100 ms 未收到可接受帧时 `STOP_WATCHDOG`。
   - 参考实现是 Python 测试模型，不是 MSPM0 固件；MCU 必须移植相同规则后才可启用二进制协议。
4. 增加协议文档与测试
   - 文档给出精确字节布局和安全决策表。
   - 测试覆盖正常、预测、丢失、过期、旧序号、CRC 错和看门狗超时。

## 涉及文件

- `protocol/ascii_aim.py`
- `protocol/binary_v1.py`
- `protocol/mspm0_safety_reference.py`
- `protocol/crc16.py`
- `config.py`
- `docs/binary_protocol.md`
- `tests/test_binary_protocol.py`
- `app.yaml`
- `.log/change/2026-07-14_add-v2-uart-protocol-safety.md`

## 验证情况

- 已运行二进制协议专项测试 4 项，通过。
- 已对工程内 64 个 Python 文件执行内存语法编译，通过。
- 已执行 `python -B -m unittest discover -s tests -v`：46 项测试全部通过。
- 未在 MSPM0G3507 固件中移植或验证二进制解析；V2 当前仍以 ASCII AIM 发送。
- 未做 MaixCAM Pro 与 MSPM0 的真实 UART 串扰、掉线、复位和 100 ms 硬件看门狗验收。

## 未处理事项

- MSPM0 固件必须实现本版本的帧头同步、长度检查、CRC-16/IBM、序号回绕比较、状态位和 100 ms 看门狗后，才能将 `UART_PROTOCOL_MODE` 改为 `binary_v1`。
- 预测帧目前默认不更新电机误差；若未来允许预测控制，必须单独评估并将 `BINARY_ALLOW_PREDICTED` 与 MCU 策略同步修改。
- ASCII 首版没有序号和 CRC，不能承担二进制 V1 的旧帧/CRC 验收要求。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\protocol\ascii_aim.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\protocol\safety.py`
- Tool: `functions.apply_patch` 生成协议、参考安全模型、文档和测试；测试命令 `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`。
