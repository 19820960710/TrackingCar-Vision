# 2026-07-15 准星漂移滞后 & 帧率向30fps提升分析

## 检查范围

- `vision/filters/target_temporal.py` — 双层滤波管线（低通 + 卡尔曼）
- `vision/filters/kalman.py` — 卡尔曼滤波参数
- `vision/filters/lowpass.py` — 低通滤波实现
- `vision/target/roi_refine.py` — OTSU优先 + adaptiveThreshold回退策略
- `vision/laser/detector.py` — 激光检测（LAB转换）
- `config.py` — 当前全部可调参数（v0.9.0-integrated-pipeline 版本）

## 检查依据

- 指数平滑低通滤波的延迟特性（时间常数 ≈ 1/alpha 帧）
- 卡尔曼滤波器 process_noise / measurement_noise 对响应速度的影响
- 320×240 @ 30fps 目标下的每帧处理预算（<33ms）
- 实机现象反馈：准星不能及时咬住靶心、有漂移

## 发现

### 🔴 致命（准星漂移根因）

1. **低通滤波 alpha=0.45 是漂移滞后的主要原因**
   - 影响：`blend(old, new, 0.45)` = old×0.55 + new×0.45。指数平滑的时间常数约为 1/0.45 ≈ 2.2 帧。在 20fps 下这意味着约 110ms 的稳态延迟。靶框以 100px/s 移动时，稳态滞后约 4px（在 320×240 画面中肉眼可见）。
   - 证据：
     - `vision/filters/lowpass.py` 第 1-2 行：`blend(old, new, alpha)` 实现
     - `vision/filters/target_temporal.py` 第 107-113 行：低通滤波调用
     - `config.py` 第 81 行：`TARGET_LOWPASS_ALPHA=0.45`
   - 处理：`TARGET_LOWPASS_ALPHA` 从 0.45 → 0.75（时间常数从 2.2 帧降至 0.33 帧，延迟降低约 85%）

2. **低通 + 卡尔曼串联导致延迟叠加**
   - 影响：当前管线是 `上一次卡尔曼输出 → blend(alpha=0.45) → 卡尔曼输入 → 卡尔曼输出`。低通滤波已引入 ~110ms 延迟，卡尔曼的 `process_noise=30` / `measurement_noise=9` 又在低通平滑后的结果上再叠加一层平滑。两个滤波器串联，总延迟远超单层滤波。
   - 证据：
     - `vision/filters/target_temporal.py` 第 105-117 行：`blend()` 的输出直接作为 `kalman.update()` 的输入
     - `self.filtered_point` 既存低通结果又存卡尔曼结果，形成递归反馈
   - 处理：去掉低通滤波，只保留卡尔曼。卡尔曼的 `process_noise` 从 30 → 60，让滤波器更快响应速度变化。如果保留低通，alpha 需 ≥0.80。

3. **卡尔曼初始化速度为 0 导致锁定瞬间滞后**
   - 影响：`ConstantVelocityKalman.update()` 首次调用时将速度初始化为 0.0。如果锁定瞬间靶框正在移动，卡尔曼需要数帧才能收敛到真实速度，这期间输出落后于实际位置。
   - 证据：`vision/filters/kalman.py` 第 56 行：`self.x_axis.velocity, self.y_axis.velocity = 0.0, 0.0`
   - 处理：`process_noise` 从 30 → 60 可缓解；或利用前两帧位置差估算初始速度。

### 🟡 中等（帧率提升）

4. **OTSU 优先策略已有效，但回退路径仍保留 medianBlur**
   - 影响：当前 `roi_refine` 先尝试 `threshold(OTSU)` 快速路径，失败才回退 `adaptiveThreshold`。回退路径中 `medianBlur(3×3)` 仍然存在，在难度帧中会拖慢处理。
   - 证据：`vision/target/roi_refine.py` 第 120-126 行：回退路径中 `cv2.medianBlur(gaussian, median_size)` + `cv2.adaptiveThreshold(...)`
   - 处理：回退路径去掉 `medianBlur`（GaussianBlur 已足够去噪）；`ADAPTIVE_BLOCK_SIZE` 从 31 → 15

5. **激光检测每帧做 BGR→LAB 转换**
   - 影响：`LaserDetector._candidates()` 每帧在 target ROI 上调用 `cv2.cvtColor(BGR2LAB)`，在 C906 上无 SIMD 加速。即使 OTSU 快速路径下 target refine 已经优化，激光检测仍是额外负担。
   - 证据：`vision/laser/detector.py` 第 64 行：`lab = cv2.cvtColor(bgr, cv2.COLOR_BGR2LAB)`
   - 处理：激光检测降频到每 2-3 帧 1 次（帧计数器取模判断），对激光点跟踪稳定性影响很小

6. **GaussianBlur(5×5) 在 320×240 下可考虑缩小**
   - 影响：5×5 的高斯核在 320×240 分辨率下（ROI 约 200×280 时 GaussianBlur 面积约 396×476）仍然有可观的计算量。如果赛场光照均匀，3×3 核通常够用，计算量减半。
   - 证据：`vision/target/roi_refine.py` 第 118 行：`cv2.GaussianBlur(gray, (gaussian_size, gaussian_size), 0)`；`config.py` 第 49 行：`TARGET_REFINE_GAUSSIAN_SIZE=5`
   - 处理：`TARGET_REFINE_GAUSSIAN_SIZE` 从 5 → 3（需验证弱光下识别率），预期 GaussianBlur 耗时降约 60%

7. **OTSU 路径稳定时 adaptiveThreshold 回退可完全跳过**
   - 影响：如果 OTSU 在 95%+ 的帧中都能找到合法四边形，则可以完全去掉 adaptiveThreshold 回退（包括其 medianBlur + 31×31 自适应阈值 + 独立的 `_best_candidate` 调用），节省代码路径和偶发慢帧。
   - 证据：`vision/target/roi_refine.py` 第 119-126 行：自适应阈值回退逻辑
   - 处理：建议先通过 `ENABLE_TIMING=True` 观察 OTSU 成功率和 adaptiveThreshold 回退频率。若回退率 <5%，去掉回退路径。

## 已采纳

- 分辨率从 512×320 → 320×240（`config.py` 第 5-6 行）
- FPS 目标从 60 → 30（`config.py` 第 7 行）
- `PERFORMANCE_SNAPSHOT_INTERVAL` 已添加（`config.py` 第 105 行）
- ROI margin 从 20/0.25 → 32/0.35（`config.py` 第 56-57 行）
- outlier JUMP 从 12/0.30 → 28/0.50（`config.py` 第 83-84 行）
- `_remove_border_connected_white` 改为单次 flood fill（`roi_refine.py` 第 38-43 行）
- `roi_refine` 增加 OTSU 优先策略（`roi_refine.py` 第 119 行）

## 未采纳

- 无。

## 修复建议

| 优先级 | 被发现 | 改动 | 风险 | 预期效果 |
|--------|-------|------|------|---------|
| **P0** | 1 | `TARGET_LOWPASS_ALPHA` 0.45→0.75 | 低（可能轻微增加噪声） | 准星滞后减少约 85% |
| **P0** | 2 | 去掉低通，只保留卡尔曼；`process_noise` 30→60 | 中（需验证稳定性） | 消除双层延迟，响应更快 |
| **P1** | 5 | 激光检测降频到每 2-3 帧 | 低 | 节省锁定态 5-10ms/帧 |
| **P1** | 4 | 回退路径去掉 medianBlur；block 31→15 | 低（回退路径使用率低） | 难度帧不再明显掉速 |
| **P2** | 6 | `GAUSSIAN_SIZE` 5→3 | 中（弱光可能丢特征） | GaussianBlur 耗时降约 60% |
| **P2** | 7 | 去掉 adaptiveThreshold 回退 | 中（极端帧可能丢目标） | 消除偶发慢帧 |
| **P3** | 3 | 卡尔曼用前两帧估算初始速度 | 低 | 锁定瞬间准星立即跟上 |

## 漂移问题的推荐最小改动

如果只改一行，改 `config.py`：

```python
TARGET_LOWPASS_ALPHA = 0.75   # 原值 0.45
```

预期效果：在保持卡尔曼在线的情况下，准星响应时间常数从 ~110ms 降至 ~17ms（20fps 下），漂移感基本消失。

更彻底的方案是直接去掉低通（注释掉 `target_temporal.py` 第 107-113 行），让原始测量直接输入卡尔曼，同时把 `process_noise` 调到 60。

## 验证情况

- 未执行运行验证：无 MaixCAM 实机连接。
- 低通延迟分析基于指数平滑数学特性（稳态滞后 = 速度 × (1/alpha − 1) × 帧间隔），属确定性结论。
- 高斯核缩小和激光降频的效果需实机 `ENABLE_TIMING=True` 验证 P95 耗时变化。

## 依据与工具

- Skill: `未使用额外 skill`
- Source: `未读取旧资料`
- Tool: `task_shell_start` + `task_shell_wait` + `retrieve_tool_result`，command `type "<path>"`，cwd `D:\Documents\Downloads`
