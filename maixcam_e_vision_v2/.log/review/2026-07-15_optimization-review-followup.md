# 2026-07-15 优化审查意见续改复核

## 检查范围

- `.log/review` 下现有 6 份审查记录及其仍未关闭的意见。
- 当前 `config.py`、ROI 精炼、目标时序、激光检测、显示、应用打包清单和相关测试。
- 本轮代码修改后的全量桌面验证结果。

## 检查依据

- 320×240 @ 30 FPS 的实时目标。
- ASCII 目标/激光量测 100 ms 新鲜度和 MSPM0 100 ms 看门狗约束。
- 当前测试对激光消失、短时预测、显示准星和 ROI 回退路径的行为契约。
- 无 MaixCAM、MSPM0 或电机实机连接的事实边界。

## 发现

1. 回退路径 `medianBlur` 建议仍成立，已采纳。
   - GaussianBlur 已在 Otsu 和 adaptiveThreshold 之前完成；重复中值滤波增加难帧开销。
   - 删除后强制回退测试仍识别有效四边形，桌面代表尺寸预处理减少约 13.4%。
2. 显示准星与控制坐标分离是有意设计，已补注释。
   - 屏幕红色准星使用当前角点几何中心，避免低通/卡尔曼造成可见滞后。
   - UART 仍读取时序处理后的 `target.x/y`，控制链没有绕过滤波器。
3. `TARGET_KALMAN_PROCESS_NOISE=1000.0` 不是启动或逻辑错误，已补注释但仍需实机观察抖动。
   - 当前桌面运动测试要求持续运动后的滞后小于 8 px并已通过。
   - 是否降到 50-200 应由实机静止抖动与快速移动数据决定，不能只凭 Q/R 数学比值改写。
4. 激光隔帧检测建议本轮不采纳。
   - 30 FPS 下隔帧只剩 15 Hz；若返回 `_last_result`，旧激光点会被当作当前帧量测。
   - `test_disappearance_returns_invalid_not_old_laser_point` 明确要求当前帧无候选时立即失效。要优化激光，应先拆分分段 P95 或改用更轻量的同帧检测，而不是复用旧控制数据。
5. `PREDICT_HOLD_MS` 提高到 100-120 ms 的建议本轮不采纳。
   - 目标状态机的 ROI 失败计数先于时序预测发生，延长 hold 不能阻止全局重捕。
   - 当前 ASCII 新鲜度和 MSPM0 看门狗都是 100 ms；120 ms 会与安全边界冲突，100 ms 又恰在临界点。保持 80 ms 更保守。
6. 两个未使用应用模块的清理意见仍成立，已采纳。
   - `app/diagnostics.py` 与生产使用的 `app/timing.py` 重复；`app/runtime_state.py` 状态职责已由运行时、状态机和时序处理器承担。
   - 已删除文件并同步 `app.yaml`，清单检查无缺失。
7. 视觉比例参考的 `_MM` 命名意见成立，已兼容迁移。
   - 新代码使用 `TARGET_ASPECT_*_REF`；旧名称保留为别名，不破坏旧消费者。
8. 仍存在硬件发布阻塞项，本轮不能关闭。
   - 默认入口尚无受控的“激光关闭背景标定”流程。
   - `PLANE_*_MM` 未填写实测尺寸，YOLO 未绑定模型，MSPM0/电机保护和长稳/帧率没有实机记录。

## 已采纳

- 删除 ROI 回退路径的重复 `medianBlur` 并增加回归测试。
- 为卡尔曼激进参数和显示/控制坐标分离补充设计注释。
- 删除 `app/diagnostics.py`、`app/runtime_state.py` 及其打包条目。
- 使用新的视觉宽高比参考名，并保留旧名称兼容层。

## 未采纳

- 不采纳复用上一帧结果的激光隔帧检测；原因是会降低时效并违反当前帧消失即失效的测试契约。
- 不采纳将预测保持直接调到 100-120 ms；原因是不能解决状态机重捕，并与 100 ms 安全新鲜度边界冲突。
- 不删除 adaptiveThreshold 兜底；原因是没有实机 Otsu 成功率和不同光照数据。
- 不凭桌面审查填写毫米标定、绑定虚构模型或宣称达到稳定 30 FPS。

## 验证情况

- 21 项相关测试通过。
- 59 项全量测试通过，耗时约 0.107 s。
- 67 个 Python 文件内存编译通过；`app.yaml` 无缺失条目。
- 桌面回退预处理微基准约减少 13.4%；MaixCAM 实机收益待测。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-review-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_v2-optimization-review.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_full-code-review.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_review-static-feedback.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\tests\test_laser_detector.py`
- Tool: `functions.exec -> shell_command`，用于全文复核、引用搜索、微基准、编译与测试。