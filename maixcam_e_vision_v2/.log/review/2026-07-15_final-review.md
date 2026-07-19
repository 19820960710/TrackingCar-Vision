# 2026-07-15 MaixCAM-E-Vision-V2 全量代码最终审查

## 检查范围

- 项目根目录 `D:\Documents\Desktop\MaixCAM-E-Vision-V2\` 下 36 个源文件
- `config.py`、`main.py`、`settings.py`
- `app/`：`pipeline.py`、`runtime.py`、`models.py`、`state_machine.py`、`timing.py`
- `drivers/`：`camera_device.py`、`uart_device.py`、`display.py`
- `vision/target/`：`detector.py`、`ai_recapture.py`、`roi.py`、`roi_refine.py`、`global_proposal.py`、`validator.py`
- `vision/geometry/`：`quadrilateral.py`、`center.py`、`standard_plane.py`、`homography.py`、`roi.py`、`circle_path.py`
- `vision/laser/`：`detector.py`、`scorer.py`、`gate.py`、`background.py`
- `vision/filters/`：`target_temporal.py`、`kalman.py`、`lowpass.py`、`outlier_gate.py`、`confirm.py`
- `protocol/`：`ascii_aim.py`、`binary_v1.py`、`crc16.py`、`safety.py`、`mspm0_safety_reference.py`

## 检查依据

- 项目内部接口契约（V2 冻结数据契约）
- MaixCAM Pro 实机约束（C906 核、无硬件 CV 加速、UART0 独占）
- 320×240 @ 30fps 实时约束
- 四级降级状态机设计意图（TRACK -> RECAPTURE -> FALLBACK -> LOST）
- project-logbook 开发审查日志模板（`D:\Documents\Desktop\diansai\project-logbook-skill-2026-07-09\skills\project-logbook\references\development-review-log.md`）

## 发现

### 一、已完成的优化（按采纳时间排列）

| 编号 | 改动 | 文件 | 当前值 | 效果 |
|------|------|------|--------|------|
| A1 | 分辨率降级 | `config.py` | 320×240 @ 30fps | 每帧像素数从 163840 降至 76800，CV 运算量减半 |
| A2 | GAUSSIAN_SIZE | `config.py` | 5→3 | GaussianBlur 耗时降约 60% |
| A3 | ADAPTIVE_BLOCK_SIZE | `config.py` | 31→15 | 回退路径自适应阈值加速 |
| A4 | ROI_MARGIN 扩大 | `config.py` | 20/0.25→32/0.35 | 减少快速移动时靶框移出 ROI |
| A5 | OUTLIER_JUMP 放宽 | `config.py` | 12/0.30→28/0.50 | 减少合法大位移被误拒为 outlier |
| A6 | LOWPASS_ALPHA 提高 | `config.py` | 0.45→0.75 | 指数平滑时间常数从 ~110ms 降至 ~17ms（20fps 下） |
| A7 | 卡尔曼参数暴露 | `config.py` | Q=1000, R=9 | 高 Q 使卡尔曼近乎直通，平滑委托给低通 |
| A8 | OTSU 优先策略 | `roi_refine.py:119-126` | OTSU 快速路径，失败回退 adaptiveThreshold | 多数帧走快速路径 |
| A9 | 回退路径去 medianBlur | `roi_refine.py:123-126` | 自适应阈值直接作用于 GaussianBlur 结果 | 回退帧不再额外消耗 |
| A10 | flood fill 优化 | `roi_refine.py:38-43` | 单次 flood fill 替代逐点遍历 | 边界去除耗时显著降低 |
| A11 | 准星几何中心分离 | `display.py:46-48` | 准星渲染用角点算术平均，UART 仍用滤波坐标 | 显示零延迟，控制仍平滑 |
| A12 | DISPLAY_VERBOSE_STATUS | `config.py` | False | 减少屏幕文字绘制开销 |
| A13 | PERFORMANCE_SNAPSHOT_INTERVAL | `config.py` | 添加 =10 | 修复启动即崩溃的 ImportError |
| A14 | PREDICT_HOLD_MS 调整 | `config.py` | 80→120 | 30fps 下覆盖 ~3.6 帧预测缓冲 |
| A15 | 激光检测降频 | `laser/detector.py` | 每 2 帧检测 1 次 | 锁定态每帧节省 BGR→LAB 转换约 5-8ms |
| A16 | import 移至顶部 | `laser/detector.py:1-8` | try/except 降级处理 | 每帧不再执行 import 检查 |

### 二、架构评价：显示与测控分离

本版本（v0.9.0-integrated-pipeline）做了一个关键的正确决策——**显示坐标与滤波坐标分离**：

```
roi_refine 原始角点 ──→ 几何中心 ──→ display.py 准星渲染（零延迟）
                   │
                   └──→ 低通(0.75) ──→ 卡尔曼(Q=1000) ──→ UART 发送（滤波后）
```

- 准星咬靶心使用原始几何数据，消除了滤波延迟
- MSPM0 控制使用滤波数据，保持了控制稳定性
- 这个设计从架构层面解决了"准星漂移"和"控制稳定"两个冲突需求
- 证据：`drivers/display.py` 第 46-48 行计算角点算术平均，第 49 行注释说明了设计意图

### 三、卡尔曼参数 Q=1000 的设计意图

- Q=1000 / R=9 → Q/R≈111，卡尔曼稳态增益约 0.90-0.95
- 效果：滤波器几乎完全信任测量，模型预测权重仅约 5-10%
- 意图：让卡尔曼"直通"，平滑工作完全交给前置低通（alpha=0.75）
- 副作用：速度估计噪声（增益高→速度修正剧烈），但由于预测输出仅在丢帧时使用，影响有限
- 证据：`config.py` 第 84 行，注释明确说明"High Q is intentional"

### 四、激光降频实现细节

- 新增 `_frame_counter`、`_laser_every_n=2`、`_last_laser_result` 三个实例变量
- `detect()` 开头递增计数器，非检测帧返回缓存的上一次结果
- 所有返回值在赋值后同时更新 `_last_laser_result` 缓存
- 降频帧若无缓存（首次调用），返回 `LASER_DECIMATED` 原因码
- 证据：`vision/laser/detector.py` 第 124-127 行及后续所有 return 语句

### 五、状态机逻辑

四级状态机（TRACK → RECAPTURE → FALLBACK → LOST）逻辑清晰：

- `TRACK`：active_roi 非空时用 `roi_refine` 在 ROI 内搜索
- `roi_fail_count >= 3` 时触发降级，先尝试 AI 模型全局重捕
- AI 不可用时回退到传统全局搜索（黑框 proposal）
- `FALLBACK` 已触达过则置 `LOST`
- 证据：`app/state_machine.py` 第 9-18 行 `next_mode()` 函数和第 85-126 行 `step()` 方法

### 六、仍可优化的低优项

1. **卡尔曼初始化速度为 0**
   - 影响：锁定瞬间若靶框正在移动，卡尔曼需数帧收敛，期间输出滞后
   - 证据：`vision/filters/kalman.py` 第 56 行 `self.x_axis.velocity = 0.0`
   - 处理：暂不处理。当前 Q=1000 使卡尔曼极快收敛，影响远小于低通延迟已消除的情况。可后续用前两帧位置差估算初始速度

2. **`target_temporal.py` 低通+卡尔曼仍串联**
   - 影响：双层滤波仍存在延迟叠加，低通(0.75)输出再经卡尔曼(Q=1000)平滑
   - 证据：`vision/filters/target_temporal.py` 第 105-117 行
   - 处理：暂不处理。准星显示已用原始几何中心（零延迟），滤波链仅影响 UART 输出，而 UART 目标受众 MSPM0 需要平滑信号。若 UART 输出滞后明显，可考虑去掉低通仅保留卡尔曼

3. **`binary_v1.py` 接口签名与 `pipeline._encode()` 不兼容**
   - 影响：二进制协议接收 `(observation, sequence, max_age_ms)`，pipeline 按 ASCII 约定调用 `encoder(target, laser)`
   - 证据：`protocol/binary_v1.py` 第 35 行 vs `app/pipeline.py` 第 49 行
   - 处理：暂不处理。`UART_PROTOCOL_MODE="ascii_aim"`，二进制未启用。等待 MSPM0 固件升级时一并修改

4. **`roi_refine.py` 的 adaptiveThreshold 回退路径可考虑完全移除**
   - 影响：如果 OTSU 在 95%+ 帧中找到合法四边形，回退路径可去掉以消除偶发慢帧
   - 证据：`vision/target/roi_refine.py` 第 123-126 行
   - 处理：暂不处理。需先通过 `ENABLE_TIMING=True` + 实机日志观察 OTSU 成功率和回退频率后再决定

## 已采纳

- A1-A16 共 16 项优化，覆盖分辨率、滤波参数、ROI 策略、图像预处理、显示逻辑、激光检测、导入优化
- 准星几何中心分离设计为架构级改进，超出单纯参数调优范畴
- 卡尔曼 Q=1000 为有意设计，注释已说明意图

## 未采纳

- 卡尔曼初始速度估算：影响小，Q=1000 已保证快速收敛
- 去掉低通仅保留卡尔曼：UART 输出需要平滑，准星显示已零延迟
- 二进制协议接口修复：二进制未启用，等待 MSPM0 固件升级
- adaptiveThreshold 回退移除：需实机数据验证 OTSU 成功率

## 验证情况

- `config.py` 语法检查通过（`py_compile`）
- `vision/laser/detector.py` 语法检查通过（`py_compile`）
- 未执行实机运行验证：无 MaixCAM Pro 硬件连接
- 所有耗时分析为基于代码路径的理论推算，需在实机上通过 `ENABLE_TIMING=True` + 每 60 帧日志输出确认各段 P95 耗时
- 以下项需实机验证：
  - 激光降频对激光点跟踪稳定性的影响（降频到 15fps 是否足够）
  - OTSU 快速路径的四边形检测成功率
  - 卡尔曼 Q=1000 时红框是否有可见高频抖动
  - PREDICT_HOLD_MS=120 在 30fps 下的预测缓冲是否充足

## 依据与工具

- Skill: `未使用额外 skill`
- Source:
  - `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_full-code-review.md`
  - `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_crosshair-lag-fps-30.md`
  - `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_v2-optimization-review.md`
  - `D:\Documents\Desktop\diansai\project-logbook-skill-2026-07-09\skills\project-logbook\references\development-review-log.md`
- Tool: `task_shell_start` + `task_shell_wait`，command `python -c "..."`，cwd `D:\Documents\Downloads`
