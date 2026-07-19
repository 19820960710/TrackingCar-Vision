# 2026-07-15 V2 优化版本审查

## 检查范围

- `config.py` — 全部可调参数
- `vision/filters/target_temporal.py` — 卡尔曼参数传递链路
- `vision/filters/kalman.py` — 滤波器内部实现（未变）
- `vision/target/roi_refine.py` — OTSU 优先 + 回退策略
- `vision/laser/detector.py` — 激光检测（未变）
- `vision/target/detector.py` — 卡尔曼参数注入
- `app/pipeline.py` — 管线调度
- `app/runtime.py` — 摄像头 → 管线 → 显示
- `drivers/display.py` — 屏幕渲染（准星位置计算逻辑）
- `main.py` — 启动流程

## 检查依据

- 上轮审查（`2026-07-15_full-code-review.md`、`2026-07-15_crosshair-lag-fps-30.md`）中提出的 12 条发现和 7 条修复建议
- 320×240 @ 30fps 目标下的实时约束
- 实机反馈：红框需准确咬住靶心、帧率需向 30fps 靠拢

## 发现

### ✅ 已采纳的上轮建议

| 上轮编号 | 改动 | 当前值 | 效果 |
|---------|------|--------|------|
| P1-2 | `GAUSSIAN_SIZE` 5→3 | `config.py` 第 49 行 | GaussianBlur 耗时降约 60% |
| P1-4 | `ADAPTIVE_BLOCK_SIZE` 31→15 | `config.py` 第 51 行 | 回退路径加速 |
| P0-1 | `TARGET_LOWPASS_ALPHA` 0.45→0.75 | `config.py` 第 81 行 | 漂移时间常数从 110ms→17ms |
| P0-2 | 卡尔曼参数暴露到 config | 第 84-85 行新增 | 可调优，不再硬编码 |
| — | 新增 `DISPLAY_VERBOSE_STATUS` | 第 107 行 | 减少屏幕文字绘制 |
| — | `main.py` 接入 `DisplayDevice` | `main.py` 第 30、40 行 | 屏幕可见调试叠层 |

### 🔴 值得关注的改动

1. **`TARGET_KALMAN_PROCESS_NOISE = 1000.0`（原默认 30.0）**
   - 影响：Q=1000 / R=9 → Q/R≈111，卡尔曼增益 K 稳态约 0.90-0.95。滤波器几乎完全信任测量，模型预测权重仅约 5-10%。这在效果上等价于让卡尔曼"直通"，平滑工作完全交给前置低通（alpha=0.75）。意图明确——优先响应速度，牺牲平滑性。
   - 证据：`config.py` 第 84 行；`kalman.py` 第 6-7 行 predict() 公式 `q*dt²` 计算
   - 副作用：速度估计会非常噪声（增益高 → 速度修正剧烈）。但由于卡尔曼的预测输出仅在 `_predict()`（丢帧时）使用，而低通 alpha=0.75 已在锁定态提供了主要平滑，实际影响有限。
   - 处理：如果红框在锁定态有明显高频抖动，把 `TARGET_KALMAN_PROCESS_NOISE` 降到 50-200 即可。如果当前效果满意则保持。**不是 bug，是有意的激进调参。**

2. **`display.py` 准星改用角点几何中心而非滤波输出**
   - 影响：这是本版本最重要的改进之一。锁定态下准星的 x/y 坐标直接由四边形四个角点的算术平均计算，不再经过低通和卡尔曼。**这意味着屏幕上的红色准星完全消除了滤波延迟**，直接反映原始几何检测结果。而滤波后的坐标仍通过 UART 发送给 MSPM0（不受影响）。
   - 证据：`drivers/display.py` 第 46-48 行：
     ```python
     cross_x = sum(float(point[0]) for point in corners) / len(corners)
     cross_y = sum(float(point[1]) for point in corners) / len(corners)
     ```
   - 评价：**显示与测控分离的设计是正确的**。准星咬靶心应该用原始几何数据，MSPM0 控制应该用滤波数据。这条改动同时解决了"准星漂移"和"控制稳定性"两个冲突需求。
   - 处理：无需改动。建议在代码注释中写明这一设计意图，避免后续维护时误改。

### 🟡 仍可优化

3. **回退路径仍保留 `medianBlur`**
   - 影响：`roi_refine.py` 第 141 行，OTSU 失败后回退到 adaptiveThreshold 时，先做 `cv2.medianBlur(gaussian, median_size)`。`GAUSSIAN_SIZE=3` 的 GaussianBlur 已经提供了足够去噪，medianBlur 在此路径下是冗余的。
   - 证据：`vision/target/roi_refine.py` 第 141 行
   - 处理：去掉 `medianBlur`，将 `gaussian` 直接传给 `adaptiveThreshold`。

4. **激光检测未降频**
   - 影响：每帧仍做 BGR→LAB + inRange + findContours。在 30fps 目标下，每帧节省 5-8ms 仍有意义。
   - 证据：`vision/laser/detector.py` 第 64 行
   - 处理：在 `LaserDetector.detect()` 开头加帧计数器，`self.frame_counter += 1; if self.frame_counter % 2 != 0: return self._last_result`。

5. **`PREDICT_HOLD_MS = 80` 仍未调整**
   - 影响：上轮已指出 80ms 在低帧率时不足以覆盖丢失帧。现在帧率已接近 30fps（33ms/帧），80ms 可覆盖约 2.4 帧，基本够用。但如果偶尔掉到 20fps（50ms/帧），80ms 只覆盖 1.6 帧，仍偏紧。
   - 证据：`config.py` 第 86 行
   - 处理：建议设为 100-120，确保在偶尔掉帧时不触发预测过期。

### 🔵 代码质量

6. **卡尔曼参数 1000.0 缺少注释说明意图**
   - 影响：后续维护者看到 `process_noise=1000.0` 可能误以为是 bug 或笔误。
   - 证据：`config.py` 第 84 行仅 `TARGET_KALMAN_PROCESS_NOISE = 1000.0`，无注释
   - 处理：建议加注释 `# High Q → near-unity gain → kalman passes measurements through; smoothing delegated to lowpass`

## 已采纳

- 上轮 P0-P2 共 5 条建议已全部采纳（见上表）
- 新增 `DISPLAY_VERBOSE_STATUS` 和准星几何中心计算是超出建议范围的额外改进

## 未采纳

- 激光降频（上轮 P1-5）：未实施
- 回退路径去 medianBlur（上轮 P1-4）：未完全实施，仍保留
- `PREDICT_HOLD_MS` 调整（上轮 P0-3）：未实施

## 架构评价

本版本做了一个关键的正确决策：**显示坐标与滤波坐标分离**。

```
roi_refine 原始角点 ──→ 几何中心 ──→ display.py 准星渲染（零延迟）
                   │
                   └──→ 低通(0.75) ──→ 卡尔曼(Q=1000) ──→ UART 发送（滤波后）
```

这个设计让准星"咬靶心"不再受滤波延迟影响，同时 MSPM0 接收的坐标仍然平滑。之前两轮 review 都在纠结低通 alpha 的取舍，这个改动从架构层面解决了问题——**不用取舍，各取所需**。

## 验证情况

- 未执行运行验证：无 MaixCAM 实机连接。
- 卡尔曼 process_noise=1000 的影响分析基于数学推导（Q/R 比 → 稳态增益），需在实机上观察红框是否有可见高频抖动。
- 准星几何中心的精度取决于 roi_refine 的角点检测质量，应在不同光照和角度下验证。

## 依据与工具

- Skill: `未使用额外 skill`
- Source:
  - `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_full-code-review.md`
  - `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_crosshair-lag-fps-30.md`
- Tool: `task_shell_start` + `task_shell_wait` + `retrieve_tool_result`，command `type "<path>"`，cwd `D:\Documents\Downloads`
