# 2026-07-14 冻结 V2 第 1 步接口契约

## 修改目标

- 将硬件接口、数据字段、安全规则和第 1 步测试场景写入 V2 权威文档，后续模块只实现该契约，避免编码过程中隐式修改接口。

## 修改内容

1. 新增 `docs/interface_contract.md`
   - 固定 A16 单向发送、A17 可选双向、UART0 115200 8N1 和 `maix_comm_method=none`。
   - 固定 ASCII AIM 帧、LOST 帧和 100 ms 主控安全看门狗职责。
   - 固定 target/laser/observation 数据字段和字段语义。
   - 列出 S1～S9 无硬件与实机测试场景。
2. 更新 README
   - README 链接接口契约，并标明第 1 步已冻结的范围。

## 涉及文件

- `docs/interface_contract.md`
- `README.md`
- `.log/change/2026-07-14_freeze-v2-interface-contract.md`

## 验证情况

- 已逐项检查接口契约与 `config.py`、`app/models.py`、`protocol/ascii_aim.py` 和 `drivers/uart_device.py` 的字段及参数一致。
- S1～S7 中的纯逻辑和 UART 无硬件模拟已在上一个接口版本完成；S8～S9 尚未在实机执行。

## 未处理事项

- MSPM0 的 100 ms 停止跟随逻辑不在本仓库，本次仅固定其接口责任。
- 双向 A17 命令、二进制 CRC 帧和 405 nm 激光阈值留待后续独立步骤。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\config.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\models.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\protocol\ascii_aim.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\drivers\uart_device.py`
- Tool: `functions.apply_patch`，生成接口契约、README 和日志，cwd `C:\tmp`
