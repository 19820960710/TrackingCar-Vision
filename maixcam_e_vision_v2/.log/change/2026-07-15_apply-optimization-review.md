# 2026-07-15 落实优化审查与清理冗余路径

## 修改目标

- 落实 `.log/review` 中仍成立、且不依赖实机参数的审查意见。
- 减少目标 ROI 自适应阈值回退路径的重复滤波开销。
- 清理已确认未使用的应用模块，降低打包与维护负担。
- 明确显示准星与 UART 控制坐标分离的设计意图，避免后续误改。

## 修改内容

1. 精简 ROI 回退预处理。
   - Otsu 快速路径失败后，直接将已完成 3×3 GaussianBlur 的图像传给 `adaptiveThreshold`。
   - 删除重复的 `medianBlur` 调用、对应参数和配置导入。
   - 保留 adaptiveThreshold 兜底，不因桌面性能数据而删除复杂光照兼容路径。
2. 消除视觉比例配置的毫米命名歧义。
   - 新增 `TARGET_ASPECT_WIDTH_REF`、`TARGET_ASPECT_HEIGHT_REF`，ROI 检测改用新名称。
   - 保留 `TARGET_WHITE_WIDTH_MM`、`TARGET_WHITE_HEIGHT_MM` 作为兼容别名，避免旧配置消费者立即失效。
   - 注释明确真正的毫米标定只能使用 `PLANE_*_MM`。
3. 记录高卡尔曼过程噪声的意图。
   - 注释说明 `TARGET_KALMAN_PROCESS_NOISE=1000.0` 是为提高运动响应，主要平滑仍由前置低通承担，并非笔误。
4. 记录显示/控制坐标分离意图。
   - 锁定态屏幕准星继续使用原始角点几何中心，避免可见滤波延迟。
   - UART 继续使用时序处理后的 `target.x/y`，不改变控制数据链。
5. 清理未使用模块。
   - 删除 `app/diagnostics.py` 和 `app/runtime_state.py`。
   - 同步删除 `app.yaml` 中对应打包条目；运行计时仍统一使用 `app/timing.py`。
6. 强化回退路径测试。
   - 测试强制 Otsu 失败，并让 `cv2.medianBlur` 一旦被调用就抛错，证明 adaptiveThreshold 回退不再依赖重复中值滤波。

## 涉及文件

- `config.py`
- `vision/target/roi_refine.py`
- `drivers/display.py`
- `tests/test_roi_refine.py`
- `app.yaml`
- 删除：`app/diagnostics.py`
- 删除：`app/runtime_state.py`

## 验证情况

- 相关测试：`python -B -m unittest tests.test_roi_refine tests.test_display tests.test_target_temporal tests.test_laser_detector -v`，21 项通过。
- 全量测试：`python -B -m unittest discover -s tests -v`，59 项通过，耗时约 0.107 s。
- 内存编译：67 个 Python 源文件全部通过；`app.yaml` 文件清单无缺失项。
- 320×240 代表尺寸下的桌面回退预处理微基准：`medianBlur + adaptiveThreshold` 约 112.41 µs，直接 `adaptiveThreshold` 约 97.40 µs，减少约 13.4%。该结果仅用于相对比较，不能换算成 MaixCAM 实机 FPS。

## 未处理事项

- 未采用激光隔帧检测：30 FPS 下会把检测降为 15 Hz，并复用旧激光结果；现有测试要求激光消失的当前帧立即输出无效量测，涉及控制安全语义。
- 未把 `PREDICT_HOLD_MS` 提高到 100-120 ms：80 ms 已覆盖约 2.4 个 30 FPS 帧；同时 ASCII 新鲜度和 MSPM0 看门狗均为 100 ms，不能在未统一协议语义前延长到边界之外。
- 激光关闭背景标定、毫米尺寸、YOLO 模型、MSPM0/电机保护和实机 30 FPS 仍需要硬件输入与验收，未在桌面环境中臆造参数。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_v2-optimization-review.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_review-static-feedback.md`
- Tool: `functions.exec -> shell_command`，用于源码检查、修改、微基准、编译与单元测试。