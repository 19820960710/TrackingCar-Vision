# 2026-07-15 MaixCAM-E-Vision-V2 深入模块审查（第二批次）

## 检查范围

- `vision/filters/kalman.py` — 恒定速度卡尔曼滤波实现
- `vision/filters/lowpass.py` — 指数平滑低通
- `vision/target/detector.py` — TargetDetector（状态机 + 时间处理器的桥接层）
- `vision/target/ai_recapture.py` — YOLO 模型适配器
- `vision/target/global_proposal.py` — 全帧黑框搜索（FALLBACK 路径）
- `app/runtime.py` — VisionRuntime 主循环
- `app/models.py` — 数据契约定义
- `protocol/ascii_aim.py` — MSPM0 ASCII 协议编码
- `protocol/safety.py` — 新鲜度检查
- `config.py` — 全局配置（GLOBAL_PROPOSAL_*、MSPM0_* 参数）

## 检查依据

- 项目内部接口契约（V2 冻结数据契约）
- MaixCAM Pro 实机约束（C906 核、无硬件 CV 加速）
- 320×240 @ 30fps 实时约束
- MSPM0 接收端时序要求
- project-logbook 开发审查日志模板（`D:\Documents\Desktop\diansai\project-logbook-skill-2026-07-09\skills\project-logbook\references\development-review-log.md`）

## 发现

### 🔴 新发现 #1：协议新鲜度窗口与预测缓冲不一致

- 影响：`PREDICT_HOLD_MS=120` 允许 temporal_processor 在 age_ms ≤ 120ms 时返回预测帧，但 `ascii_aim.encode(max_age_ms=100)` 硬编码 100ms 作为新鲜度上限。在 age_ms ∈ [101, 120] 范围内，temporal 输出有效预测 target，但协议编码器因 `is_fresh(target, 100)` 返回 False 而输出 `AIM,0,0,0,0,0,0,0,LOST,LOST`。MSPM0 会比预期更早收到控制信号中断。
- 证据：
  - `config.py` 第 86 行：`PREDICT_HOLD_MS = 120`
  - `protocol/ascii_aim.py` 第 6 行：`def encode(target, laser, max_age_ms=100):`
  - `protocol/safety.py` 第 5-7 行：`is_fresh()` 使用 `age_ms <= max_age_ms`
  - `app/pipeline.py` 第 48-50 行：`_encode()` 调用 `encoder(target, laser)`，不传 max_age_ms，使用默认值 100
- 处理：在 `pipeline._encode()` 中传入 `max_age_ms` 参数。config 中已有 `MSPM0_STALE_MS = 100`，应将其作为权威值传递给协议编码器。具体修改：`app/pipeline.py` 第 49 行改为 `return encoder(target, laser, max_age_ms=MSPM0_STALE_MS)`（需 import MSPM0_STALE_MS）。

### 🔴 新发现 #2：MSPM0_STALE_MS 定义了但未被任何模块读取

- 影响：`config.py` 第 104 行定义了 `MSPM0_STALE_MS = 100`，但全文搜索无任何 `from settings import MSPM0_STALE_MS` 或直接使用。该参数的存在暗示了新鲜度窗口应该是可配置的，但实际协议编码器硬编码了默认值 100。未来调整 PREDICT_HOLD_MS 时容易忘记同步修改协议侧。
- 证据：
  - `config.py` 第 104 行：`MSPM0_STALE_MS = 100`
  - `protocol/ascii_aim.py` 全文：仅 `max_age_ms=100` 硬编码
  - `app/pipeline.py` 全文：无 MSPM0_STALE_MS 引用
- 处理：与发现 #1 一同修复——让 `pipeline._encode()` 读取 MSPM0_STALE_MS 并传递给 `encoder()`。

### 🟡 新发现 #3：GLOBAL_PROPOSAL_WIDTH 从 160 改为 320

- 影响：当前 `GLOBAL_PROPOSAL_WIDTH = 320`，在 320×240 分辨率下 scale=1.0，不做缩小。black-frame proposal 在全分辨率上做 `cv2.inRange` + `findContours`，如果频繁触发 FALLBACK 状态，每帧增加约 76800 像素的阈值和轮廓搜索。
- 证据：
  - `config.py` 第 65 行：`GLOBAL_PROPOSAL_WIDTH = 320`
  - `vision/target/global_proposal.py` 第 63 行：`scale = min(1.0, float(settings["width"]) / full_width)` — 当 width=320, full_width=320 时 scale=1.0
  - 之前审查记录（`2026-07-15_full-code-review.md`）中为 160
- 处理：如果 FALLBACK 触发频率低（<5% 帧），320 可接受（搜索质量更高）。如果 FALLBACK 频繁触发导致帧率下降，考虑降回 160。需实机 `ENABLE_TIMING=True` 观察 `GLOBAL_PROPOSAL` 耗时。

### 🟡 新发现 #4：GLOBAL_PROPOSAL_MIN_AREA_RATIO 从 0.03 改为 0.003

- 影响：最小值降低了 10 倍，意味着全局搜索将接受更小的黑色区域作为候选。在 320×240 下，0.003 对应约 230 像素，约 15×15 的正方形。这可能在复杂背景中产生更多误检，增加 Fallback->Refine 链中的无效 ROI 精炼。
- 证据：
  - `config.py` 第 67 行：`GLOBAL_PROPOSAL_MIN_AREA_RATIO = 0.003`
  - 之前审查记录中为 0.03
- 处理：如果误检多（日志中频繁出现 `CLASSICAL_GLOBAL_REFINE_FAILED`），提高至 0.01-0.02。

### 🟢 代码质量确认

| 模块 | 评价 |
|------|------|
| `kalman.py` | 干净。无 NumPy 依赖，dt 上限 0.2s 在 PREDICT_HOLD_MS=120 下安全。初始化速度=0 已在前述报告标注 |
| `lowpass.py` | 单行 `blend()`，正确 |
| `models.py` | 数据契约完整。`normalize_measurement` 拒绝未知字段（白名单模式），字段校验严格 |
| `ai_recapture.py` | 接口干净。`model=None` 时自动返回 `MODEL_UNAVAILABLE`，外部模型注入无侵入 |
| `runtime.py` | `capture`/`display`/`frame_total` 三段计时分离清晰。`run_once` 只做调度，不耦合算法 |
| `global_proposal.py` | 有 `cv2 is None` 和 `_DEFAULT_SETTINGS is None` 双守卫，鲁棒性好 |
| `detector.py` | `_TimedAiRecapture` 包裹器精细计时 AI 重捕耗时。桥接层职责单一 |

## 已采纳

- 无。首次审查本批次模块。

## 未采纳

- GLOBAL_PROPOSAL_WIDTH=320 暂不调整：若 FALLBACK 频率低则影响可接受，需实机数据决策
- GLOBAL_PROPOSAL_MIN_AREA_RATIO=0.003 暂不调整：同上

## 验证情况

- 未执行实机运行验证：无 MaixCAM Pro 硬件连接
- 协议新鲜度不一致为代码级确定性问题，通过行号对证即可确认，无需实机
- GLOBAL_PROPOSAL 参数变更的影响需实机 `ENABLE_TIMING=True` + 日志验证

## 依据与工具

- Skill: `未使用额外 skill`
- Source:
  - `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_full-code-review.md`
  - `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_final-review.md`
  - `D:\Documents\Desktop\diansai\project-logbook-skill-2026-07-09\skills\project-logbook\references\development-review-log.md`
- Tool: `task_shell_start` + `task_shell_wait`，command `python -c "..."`，cwd `D:\Documents\Downloads`
