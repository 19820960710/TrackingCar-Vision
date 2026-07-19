# 2026-07-15 显式绑定 UART 新鲜度配置

## 修改目标

- 消除 `MSPM0_STALE_MS` 只定义但未进入生产编码链路的问题。
- 保持当前 100 ms UART 安全行为、Pipeline 通用协议接口和 120 ms 屏幕预测行为不变。

## 修改内容

1. 在组合入口增加 `encode_ascii_for_mspm0(target, laser)`。
   - 从 `settings` 读取 `MSPM0_STALE_MS`。
   - 调用 `ascii_encode(target, laser, max_age_ms=MSPM0_STALE_MS)`。
   - `VisionPipeline` 仍只依赖通用二参数协议函数，不强制所有协议适配器接受 ASCII 专用关键字。
2. `build_runtime()` 改用配置绑定后的 ASCII 适配器。
3. 在配置中说明安全分层。
   - `PREDICT_HOLD_MS=120` 可供屏幕/时序预测继续显示。
   - UART 量测仍由 `MSPM0_STALE_MS=100` 截止，且不得超过 `MSPM0_WATCHDOG_MS=100`。
4. 增加协议边界测试。
   - `age_ms=100` 时仍输出有效 AIM。
   - `age_ms=101` 时输出 LOST。

## 涉及文件

- `main.py`
- `config.py`
- `tests/test_protocol.py`

## 验证情况

- 67 个 Python 文件内存语法编译通过。
- 协议、Pipeline、安全和目标时序相关测试 16 项通过。
- 全量 `python -B -m unittest discover -s tests` 共 67 项通过，耗时约 0.121 s。
- 当前配置值仍为 100 ms，因此生产串口输出与修改前一致；本次修复的是配置权威性和未来可维护性。
- 未执行 MaixCAM/MSPM0 实机串口验收；需要主控确认 101 ms 旧量测不会刷新看门狗。

## 未处理事项

- 未将 `MSPM0_STALE_MS` 提高到 120；会超过当前 100 ms 主控看门狗安全窗口。
- 未把 `PREDICT_HOLD_MS` 降到 100；屏幕预测与 UART 控制采用有意分层。
- 未修改 `GLOBAL_PROPOSAL_WIDTH=320` 或 `GLOBAL_PROPOSAL_MIN_AREA_RATIO=0.003`，避免削弱远距离小黑框首捕。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_deep-module-review.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\main.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\protocol\ascii_aim.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\protocol\safety.py`
- Tool: `functions.exec -> shell_command`，commands 为内存编译、相关测试和全量 unittest，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`