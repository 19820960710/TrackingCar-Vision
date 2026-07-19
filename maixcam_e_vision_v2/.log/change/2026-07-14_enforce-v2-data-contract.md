# 2026-07-14 强制 V2 统一数据契约与模块边界

## 修改目标

- 将第 1 步“统一量测结构、模块只收发统一数据、视觉不控制电机”的完成标准落实为可执行归一化代码、单元测试和中文架构文档。

## 修改内容

1. 强制统一量测字段
   - `app/models.py` 定义全部固定字段和默认值。
   - 新增 `normalize_measurement()`；拒绝未知字段，补齐缺省字段，约束 `age_ms` 非负、`confidence` 在 0～100。
   - `new_observation()` 统一归一化 target 与 laser，并保存帧序号和时间戳。
2. 固化流水线边界
   - `app/pipeline.py` 在调用检测器后立即归一化 target，再将统一 target 传给激光检测器。
   - 视觉模块不导入 drivers、protocol 或 MSPM0 层。
3. 新增可脱离硬件的测试
   - 测试量测字段完整性、默认值、非法字段拒绝、范围约束和 observation 组合。
   - 测试流水线在目标模块和激光模块之间传递完整 target 字段。
   - 静态检查 `vision/` 不导入硬件或协议层。
4. 更新中文架构说明
   - 写明 app、drivers、vision、protocol、tests 的职责和禁止事项。

## 涉及文件

- `app/models.py`
- `app/pipeline.py`
- `tests/test_models.py`
- `tests/test_module_boundary.py`
- `tests/test_pipeline.py`
- `docs/architecture.md`
- `.log/change/2026-07-14_enforce-v2-data-contract.md`

## 验证情况

- 对仓库中 49 个 Python 文件执行内存语法编译，全部通过。
- 执行 `python -m unittest discover -s tests -v`，14 个无硬件测试全部通过。
- 已覆盖数据字段完整性、字段归一化、未知字段拒绝、流水线交接、视觉层导入边界、几何、协议和状态机。
- 未在 MaixCAM Pro、MSPM0G3507 或张大头电机上运行；本次只验证模块边界和纯逻辑。

## 未处理事项

- 靶框、激光、几何和滤波算法尚未实现；本次只冻结数据和模块边界。
- MSPM0 停止跟随逻辑仍在主控工程，本仓库不直接控制电机。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\docs\interface_contract.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\models.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\pipeline.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\docs\architecture.md`
- Tool: `functions.apply_patch`，生成数据契约、测试、架构文档和日志，cwd `C:\tmp`
- Tool: `functions.shell_command`，执行语法编译和 `python -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
