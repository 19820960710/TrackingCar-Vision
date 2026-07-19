# 2026-07-15 静态审查反馈复核

## 检查范围

- 用户提供的 10 条反馈，涉及配置导入、未使用模块、预测守卫、AI 回退日志、二进制协议接口、OpenCV 导入、包初始化和配置命名。
- 当前磁盘工程 `D:\Documents\Desktop\MaixCAM-E-Vision-V2`；未检查 MaixCAM 设备上已部署文件的实际内容。

## 检查依据

- 当前源文件、`app.yaml` 打包清单、源码引用搜索和 53 项桌面单元测试。
- 区分当前运行风险、未来协议启用风险、测试/验收辅助 API 和仅影响可维护性的未使用代码。

## 发现

1. `PERFORMANCE_SNAPSHOT_INTERVAL` 未定义：当前工作区不成立，设备部署一致性需要验证。
   - 影响: 当前磁盘代码可导入；若设备只更新了 `app/pipeline.py` 而保留旧 `config.py`，设备仍会在导入时失败。
   - 证据: `config.py:91` 定义 `PERFORMANCE_SNAPSHOT_INTERVAL = 10`；`app.yaml` 同时包含 `config.py`、`settings.py`、`app/pipeline.py`；导入冒烟输出 `interval=10 import=OK`。
   - 处理: 不采纳“当前源码缺常量”和改为 60 的结论；需要重新打包确认设备文件同批部署。值 10 是当前 30 FPS 调试界面的约 3 Hz P95 更新策略。
2. `app/diagnostics.py` 未使用且与计时实现重复：属实。
   - 影响: 增加打包文件和维护面，不影响当前运行。
   - 证据: 源码无 `app.diagnostics` 导入；`app.yaml` 仍打包该文件；实际运行使用 `app/timing.py::PerformanceStats`。
   - 处理: 采纳清理方向。反馈中“PerformanceStats 使用线性插值”不准确；两个实现都以排序后的离散索引取值。后续删除时应同时移除 `app.yaml` 条目。
3. `app/runtime_state.py` 未使用：属实。
   - 影响: 状态职责重复，增加理解成本，不影响当前运行。
   - 证据: 源码无 `RuntimeState` 引用；实际状态位于 `VisionRuntime`、`TargetRecoveryStateMachine` 和 `TargetTemporalProcessor`。
   - 处理: 采纳清理方向；后续删除时同步移除 `app.yaml` 条目。
4. `circle_radius_error_mm()` 是死代码：不完全成立。
   - 影响: 生产主循环未调用，但它用于圆轨迹毫米误差验收。
   - 证据: `tests/test_standard_plane.py:34` 调用该方法并验证投影误差。
   - 处理: 不采纳删除；保留为标定/验收 API，可补充 docstring 说明非运行时路径。
5. `_predict()` 的 `point is None` 防御守卫：建议有效，但正常状态不触发。
   - 影响: 若内部状态被错误恢复、手工修改或未来重构破坏不变量，可能在 `point[0]` 抛出 `TypeError`。
   - 证据: 公共更新流程只有在确认成功并设置 `filtered_point` 后才写入 `last_real`；当前两者按流程同步存在。
   - 处理: 采纳防御性修复方向；应增加守卫和内部状态不一致测试。
6. YOLO 模型未加载时增加日志：部分采纳。
   - 影响: 当前回退行为正确，但用户不易区分“模型未绑定”和“模型未检出”。
   - 证据: `AiRecapture.available()` 在 `model is None` 时为假，状态机进入经典提议。
   - 处理: 不采纳在逐帧分支直接 `print`，否则丢失期间可能重复打印；应采用启动时一次性日志或状态字段/屏幕提示。
7. 二进制 `encode()` 与流水线调用不兼容：属实，但当前被显式阻断。
   - 影响: 直接把协议切到 `binary_v1` 会因参数语义不一致而失败。
   - 证据: ASCII 签名为 `encode(target, laser, max_age_ms=100)`；二进制签名为 `encode(observation, sequence, max_age_ms=100)`；流水线固定调用 `encoder(target, laser)`；`main.py` 当前拒绝非 `ascii_aim` 模式。
   - 处理: 采纳未来修复方向；在 MSPM0 二进制固件验收前不启用，应引入协议适配器和序号所有权。
8. 激光检测每帧内部 import：事实成立，但当前无证据表明是有效瓶颈。
   - 影响: Python 模块已缓存，后续 import 主要是名称查找；移到模块顶部若无降级保护，会把“激光不可用”扩大为“整个应用导入失败”。
   - 证据: `_candidates()` 内部导入 `cv2`、`numpy`，并在异常时返回空候选。
   - 处理: 暂不采纳直接顶层 import；只有实机剖析证明有收益时，才改为模块级 `try/except` 缓存并保留降级行为。
9. 缺少子模块 `__init__.py`：不成立。
   - 影响: 无此问题。
   - 证据: `vision/filters`、`vision/geometry`、`vision/laser`、`vision/target` 均存在 `__init__.py`；前三者还导出符号，`vision/target/__init__.py` 含模块 docstring。空 `__init__.py` 本身也是合法包文件。
   - 处理: 不采纳。
10. `TARGET_WHITE_*_MM` 命名容易误导：属实，但直接重命名会影响兼容性。
   - 影响: 容易与真实 `PLANE_*_MM` 标定值混淆。
   - 证据: 配置注释明确其仅用于视觉宽高比，`roi_refine.py` 用它们计算 expected aspect。
   - 处理: 采纳命名改进方向；建议先增加 `TARGET_ASPECT_WIDTH_REF`/`HEIGHT_REF`，保留旧名兼容层，再逐步迁移。

## 已采纳

- 确认 `app/diagnostics.py`、`app/runtime_state.py` 为未使用模块，适合在后续代码修改中连同 `app.yaml` 条目清理。
- 确认 `_predict()` 值得增加内部状态防御守卫和测试。
- 确认二进制协议接口在未来启用前必须增加适配层。
- 确认 AI 模型不可用需要一次性、非刷屏的可见状态。
- 确认视觉比例配置命名需要兼容迁移。
- 本次仅复核审查意见，未修改功能代码；原因是用户本轮提供反馈但未明确授权执行这些结构性修改。

## 未采纳

- 不采纳“当前工作区缺少 `PERFORMANCE_SNAPSHOT_INTERVAL`”；当前值为 10 且导入通过。设备若报错应检查部署版本混用。
- 不采纳删除 `circle_radius_error_mm()`；它由标准平面测试使用并承担验收用途。
- 不采纳在状态机逐帧回退分支直接打印模型不可用信息；会有日志刷屏风险。
- 不采纳未经实机剖析就将 OpenCV/NumPy 改为无保护顶层导入。
- 不采纳“缺少子模块 `__init__.py`”；文件实际存在。

## 验证情况

- `python -B -c "import config; from app.pipeline import VisionPipeline; ..."`：输出 `interval=10 import=OK`。
- `python -B -m unittest discover -s tests -v`：53 项测试通过。
- 未读取 MaixCAM 设备上的部署目录，无法证明设备端 `config.py` 与 `pipeline.py` 版本一致。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-review-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\config.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app.yaml`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\pipeline.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\diagnostics.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\runtime_state.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\filters\target_temporal.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\geometry\standard_plane.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\protocol\binary_v1.py`
- Tool: `functions.exec -> shell_command`，command `python -B -c ...`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
- Tool: `functions.exec -> shell_command`，command `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
