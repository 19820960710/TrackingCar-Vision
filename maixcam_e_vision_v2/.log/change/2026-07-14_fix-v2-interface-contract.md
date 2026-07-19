# 2026-07-14 固化 V2 第 1 步硬件与安全接口

## 修改目标

- 在独立 V2 仓库中固化不变的硬件连接、UART0 占用规则、首版文本协议和视觉侧安全状态；不修改旧版仓库。

## 修改内容

1. 固定 UART0 单向默认配置
   - 保持 A16 为 UART0_TX、`/dev/ttyS0`、115200、8N1。
   - 增加 `UART_ENABLE_RX=False`；默认不映射 A17，只有明确启用双向命令时才映射 A17/UART0_RX。
   - UART 初始化继续要求 `maix_comm_method=none`，否则拒绝打开 UART0。
2. 固定首版协议与安全契约
   - 保持 ASCII `AIM,...` 协议兼容现有 MSPM0。
   - 新增协议层新鲜度判断：无效、`lost_hold=True` 或 `age_ms>100` 的目标不再作为有效靶标发送，输出 LOST 帧。
   - 记录 MSPM0 仍需独立实现 100 ms 接收看门狗；本次未修改 MSPM0 代码。
3. 修正激光字段默认值
   - 根据 2025 E 题推荐的 405 nm 蓝紫激光，ASCII 默认颜色从 `green` 改为 `blue_violet`。
4. 更新中文说明和包版本
   - README 与 UART 文档改为中文固定接口说明。
   - V2 版本更新为 `0.1.1-interface-contract`，应用元数据为 0.1.1。

## 涉及文件

- `config.py`
- `drivers/uart_device.py`
- `protocol/ascii_aim.py`
- `protocol/safety.py`
- `app.yaml`
- `README.md`
- `docs/uart_protocol.md`
- `tests/test_protocol.py`
- `.log/change/2026-07-14_fix-v2-interface-contract.md`

## 验证情况

- 对仓库中 46 个 Python 文件执行内存语法编译，全部通过。
- 执行 `python -m unittest discover -s tests -v`，8 个纯逻辑测试全部通过。
- 使用假 MaixPy 模块验证：默认只映射 A16/UART0_TX；UART0 115200 创建正确；系统通信方式不是 `none` 时初始化被拒绝。
- 未在 MaixCAM Pro、MSPM0G3507 和张大头电机上实机发送或接收。

## 未处理事项

- MSPM0 接收看门狗和停止控制逻辑属于主控工程，本次未修改。
- A17 双向命令、二进制 CRC 协议、状态位和 `age_ms` 的在线传输留在后续独立协议版本。
- 405 nm 蓝紫激光的相机可见性、颜色特征和阈值必须实机采样后实现。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\电赛历年真题\2025\E题_简易自行瞄准装置.pdf`
- Source: `D:\Documents\Desktop\MaixPy官方E题_最终方案选型.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\config.py`
- Tool: `functions.apply_patch`，生成第 1 步接口文件和变更日志，cwd `C:\tmp`
- Tool: `functions.shell_command`，执行语法、单元测试和假 MaixPy UART 映射测试，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
