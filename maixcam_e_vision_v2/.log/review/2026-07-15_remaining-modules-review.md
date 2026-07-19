# 2026-07-15 MaixCAM-E-Vision-V2 剩余模块审查（第三批次）

## 检查范围

- `vision/geometry/quadrilateral.py` — 四边形角点排序与度量
- `vision/geometry/center.py` — 对角线交点中心计算
- `vision/geometry/roi.py` — 图像范围 ROI 裁剪
- `vision/geometry/homography.py` — 纯 Python 单应性矩阵求解
- `vision/geometry/standard_plane.py` — 毫米坐标映射与缓存
- `vision/filters/outlier_gate.py` — 异常值距离门控
- `vision/filters/confirm.py` — 连续帧确认计数器
- `vision/laser/gate.py` — 激光跳变门控
- `vision/laser/scorer.py` — 激光候选加权评分
- `vision/laser/background.py` — 静态亮点背景模型
- `vision/target/roi.py` — 跟踪 ROI 扩展计算
- `vision/target/validator.py` — 角点凸性验证
- `app/timing.py` — 微秒计时与 P95 统计
- `drivers/camera_device.py` — 相机初始化与调优
- `drivers/uart_device.py` — UART0 引脚映射与所有权检查
- `settings.py` — 配置导入面
- 其余 `__init__.py`（不含逻辑）

## 检查依据

- 项目内部接口契约（V2 冻结数据契约）
- MaixCAM Pro 实机约束（C906 核、无硬件 CV 加速、无 NumPy）
- 320×240 @ 30fps 实时约束
- project-logbook 开发审查日志模板（`D:\Documents\Desktop\diansai\project-logbook-skill-2026-07-09\skills\project-logbook\references\development-review-log.md`）

## 发现

### 1. `quadrilateral.py` — 四边形几何实现严谨

- 影响：正面的。`order_corners()` 使用 atan2 角排序 + 凸性验证 + 退化检测（四点不共线、无重复点），`EPSILON=1e-9` 阈值合理。`_is_convex_ordered()` 确保所有叉积同号。`is_convex_quad()` 委托给 `order_corners()` 来做完整验证而非简单检查——每次都会重新排序，计算量稍大但安全正确。整个模块无任何外部依赖。
- 证据：`vision/geometry/quadrilateral.py` 全文 87 行
- 处理：无需改动。如果后续性能分析显示 `is_convex_quad()` 是热点（每次调用都重新排序），可缓存排序结果或改为只做凸性检查不排序。

### 2. `homography.py` — 纯 Python 8×8 求解器

- 影响：`compute()` 构建 8×8 增广矩阵 + Gauss-Jordan 列主元消元，`invert()` 解析求 3×3 逆矩阵，`project()` 标准齐次投影。全部纯 Python，无 NumPy。在 C906 上每次求解约 500-1000 次浮点运算。`standard_plane.py` 的缓存策略（角点移动 ≤ epsilon 时复用）缓解了频繁调用。
- 证据：`vision/geometry/homography.py` 全文 52 行
- 处理：无需改动。缓存策略已在 `standard_plane.py` 中实现。如果以后需要频繁重算（如靶框持续运动），可考虑用 C 扩展或预计算查找表。

### 3. `vision/target/roi.py` — `tracking_roi()` 可能返回无效 ROI

- 影响：`tracking_roi()` 使用 `max(0, x-margin)` 和 `min(image_width, x+width+margin)` 做边界裁剪，但当矩形完全移出画面时，可能返回 `x0 >= x1` 导致宽/高 ≤ 0。不过调用方 `state_machine.py` 通过 `clip_roi()` 做了二次裁剪——`clip_roi()` 在 `right <= left` 时返回 None，调用方会正确处理。因此实际影响为零。
- 证据：
  - `vision/target/roi.py` 第 4-9 行：无内部有效性检查
  - `app/state_machine.py` 第 91-93 行：`clipped_roi = clip_roi(...)` + None 检查
- 处理：暂不处理。调用链已有防护。如果未来有新调用者直接使用 `tracking_roi()`，建议在函数内添加 `if x1 <= x0 or y1 <= y0: return None`。

### 4. `laser/background.py` — 背景模型 O(n) 搜索

- 影响：`observe()` 和 `is_static()` 中，每个候选点对已记录背景点做线性搜索（`for record in self.points`）。通常背景亮点数 < 10，O(n) 开销可忽略。若极端场景（如复杂背景 50+ 亮点），每帧搜索耗时可能达到微秒级但仍在预算内。
- 证据：`vision/laser/background.py` 第 21-28 行（observe 搜索）、第 44-46 行（is_static 搜索）
- 处理：无需改动。背景亮点数在正常场景下很小。

### 5. `laser/scorer.py` — 评分权重合理

- 影响：候选评分 = 0.20×面积分 + 0.45×圆形度 + 0.35×密度。圆形度权重最高（激光点应为近圆形），密度次之（排除空心轮廓），面积最低。权重分配符合物理特征。
- 证据：`vision/laser/scorer.py` 第 11-13 行
- 处理：无需改动。

### 6. `app/timing.py` — 环形缓冲区 + 百分位统计

- 影响：`PerformanceStats` 使用环形缓冲区（默认 120 样本）+ `percentile()` 计算 P50/P95。`add()` 在缓冲区满后循环写入。`snapshot()` 生成所有指标的摘要字典。设计正确，适合嵌入式环境（固定内存）。
- 证据：`app/timing.py` 第 24-59 行
- 处理：无需改动。

### 7. `drivers/camera_device.py` — 鲁棒性好

- 影响：`create_camera()` 有 `buff_num` 参数兼容性处理（`try/except TypeError` 回退）。`apply_initial_tuning()` 所有设置调用均有异常捕获和日志输出。`warmup()` 优先 `skip_frames()`，回退循环读取。异常不会导致启动崩溃。
- 证据：`drivers/camera_device.py` 全文 50 行
- 处理：无需改动。

### 8. `drivers/uart_device.py` — UART0 冲突检查

- 影响：`create_uart()` 检查 `comm_method=none` 防止 MaixVision 通信占用 UART0，抛出明确错误信息。引脚映射使用 `err.check_raise` 确保失败时立即终止。RX 可禁用（`UART_ENABLE_RX=False`），节省引脚。
- 证据：`drivers/uart_device.py` 第 6-11 行
- 处理：无需改动。

### 🟢 其余模块快速评估

| 模块 | 行数 | 评价 |
|------|------|------|
| `center.py` | 15 | 对角线交点公式正确，退化时返回 None |
| `roi.py (geometry)` | 17 | floor/ceil 保守裁剪，无交集返回 None |
| `outlier_gate.py` | 5 | 简单距离检查，单行逻辑 |
| `confirm.py` | 9 | 连续帧计数器，matched=False 时重置 |
| `laser/gate.py` | 9 | 从 config 读取跳变限制 |
| `validator.py` | 5 | 委托给 `is_convex_quad()` |
| `settings.py` | 3 | 仅 `from config import *`，配置入口清晰 |
| `standard_plane.py` | 117 | 严格参数验证，缓存策略，死代码 `circle_radius_error_mm()` 已在前述报告标注 |

## 已采纳

- 无。首次审查本批次模块。

## 未采纳

- `tracking_roi()` 内部有效性检查：调用链已有 `clip_roi()` 防护
- `background.py` O(n) 搜索优化：n 很小，无实际收益
- `is_convex_quad()` 排序缓存：非热点，当前实现正确优先

## 验证情况

- 未执行实机运行验证：无 MaixCAM Pro 硬件连接
- 本批次模块多为纯几何/数据结构代码，逻辑正确性可通过单元测试验证（项目含 `tests/` 目录，但未在本轮运行）
- `homography.py` 正确性可通过 `invert(compute(A,B))` 投影 A→B→A 验证数值稳定性

## 依据与工具

- Skill: `未使用额外 skill`
- Source:
  - `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_final-review.md`
  - `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_deep-module-review.md`
  - `D:\Documents\Desktop\diansai\project-logbook-skill-2026-07-09\skills\project-logbook\references\development-review-log.md`
- Tool: `task_shell_start` + `task_shell_wait`，command `python -c "f=open(...);print(f.read())"`，cwd `D:\Documents\Downloads`
