# 2026-07-15 MaixCAM-E-Vision-V2 全量代码审查

## 检查范围

- 项目主入口、配置、应用层、驱动层、视觉算法与协议源文件。
- 用户附件 `pasted-text.txt` 的 12 项发现及修复顺序。
- 当前磁盘版本；未读取 MaixCAM 设备内文件。

## 检查依据

- `docs/interface_contract.md`、状态机与 UART 安全约束。
- 当前源码、打包清单、桌面微基准和 57 项单元测试。
- 实机反馈：锁定后 FPS <10、快速移动丢目标。

## 发现

1. 附件所述 `PERFORMANCE_SNAPSHOT_INTERVAL` 缺失已过时。
   - 影响: 当前工作区可导入；设备若仍报错，说明新旧文件混合部署。
   - 证据: `config.py` 已定义为 10；导入输出 `interval=10 import=OK`；`app.yaml` 同时打包配置和流水线。
   - 处理: 不改为 60；值 10 用于 30 FPS 下约每 0.33 秒刷新 P95。
2. ROI 精炼热点属实，当前已修复。
   - 影响: 清晰目标走快速路径，复杂光照才走原自适应兜底。
   - 证据: 当前采用单次 padded flood fill、Otsu 快路和 `medianBlur + adaptiveThreshold` 兜底；桌面合成样例从约 702.9 µs 降至 271.8 µs。
   - 处理: 已采纳；设备收益需要实测。
3. `PREDICT_HOLD_MS 80→200` 不能阻止全局重捕，并涉及安全。
   - 影响: 预测保持只影响时序输出；原始 ROI 失败计数在此前独立驱动重捕。ASCII 量测超过 100 ms 仍会被判旧。
   - 证据: `state_machine.py` 先运行，`target_temporal.py` 后运行；`protocol/safety.py` 使用 100 ms 新鲜度。
   - 处理: 暂不采纳直接提高到 200-250。
4. 快速移动门控已调整。
   - 影响: 减少离开 ROI 和滤波滞后造成的误拒。
   - 证据: margin 为 32/0.35，jump 为 28/0.50，跳变参照上一原始检测点。
   - 处理: 已采纳；暂不扩大到 0.40，先实测性能与丢框率。
5. 激光 LAB 转换仍是潜在开销。
   - 影响: 目标有效时每帧执行 LAB 转换和轮廓提取。
   - 证据: `vision/laser/detector.py::_candidates()`。
   - 处理: 需要实测；30 FPS 下每两帧检测仅为 15 FPS，还涉及旧激光量测安全语义，暂不盲目降频。
6. 标准平面 0.75 px 缓存阈值可能触发重算，但几何当前未启用。
   - 证据: `PLANE_WHITE_*` 为 `None`。
   - 处理: 需要毫米标定误差数据后再调整。
7. `app/diagnostics.py` 与 `app/runtime_state.py` 未被生产代码使用。
   - 影响: 增加维护面，不影响运行。
   - 处理: 采纳后续清理方向；删除时同步修改 `app.yaml`，本次不删除。
8. `circle_radius_error_mm()` 不是完全死代码。
   - 证据: `tests/test_standard_plane.py` 用它验证毫米圆轨迹误差。
   - 处理: 不删除，保留为验收 API。
9. 二进制协议接口不兼容属实，但当前被 `main.py` 显式阻断。
   - 处理: 等 MSPM0 二进制固件实现后统一增加协议适配和序号管理。
10. `_predict()` 空点守卫已修复并有测试。
    - 处理: 已采纳。
11. 激光函数内 import 属实，但优先级低。
    - 处理: 暂不改为无保护顶层 import；只有实机剖析证明有收益时才做模块级 `try/except` 缓存。
12. 仍有发布前缺口。
    - 影响: 激光背景标定未由主入口执行、YOLO 未绑定、毫米尺寸未填、MSPM0/电机安全及长稳未实测。
    - 处理: 需要实机和外部固件验收。

## 已采纳

- 性能快照间隔与缓存、单次 flood fill、Otsu 快路与自适应兜底。
- 快速移动 ROI/跳变门限、原始点门控和预测状态守卫。
- 对应快速路径、回退路径、快速移动和异常状态测试。

## 未采纳

- 不采纳“当前缺少快照常量”及固定改成 60。
- 不采纳仅靠延长预测到 200-250 ms 阻止重捕。
- 不取消自适应阈值兜底，不在无分段 P95 时盲目降低激光检测频率。
- 不删除圆误差验收 API，不采用无保护顶层 OpenCV 导入。

## 验证情况

- 导入冒烟：`interval=10 import=OK`。
- `python -B -m unittest discover -s tests -v`：57 项通过。
- 桌面微基准仅用于改前/改后相对比较；MaixCAM 的锁定 FPS、P95、移动丢框率和长稳仍需实测。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-review-log.md`
- Source: `C:\Users\Lenovo\.codex\attachments\8a4ec88d-3485-47a6-8c76-ceb5bd71ed1c\pasted-text.txt`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\config.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\pipeline.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\target\roi_refine.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\filters\target_temporal.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\change\2026-07-15_optimize-locked-fast-tracking.md`
- Tool: `functions.exec -> shell_command`，command `python -B -c ...` 与 `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
