# 2026-07-15 调整准星响应与 30 FPS 锁定路径

## 修改目标

- 让屏幕红色准星准确位于当前帧矩形靶框中心，降低控制点移动滞后，并继续减少锁定状态显示和预处理开销。

## 修改内容

1. 分离显示准星与控制滤波点。
   - 当前帧有效目标的红色准星使用四角几何中心，矩形回退时使用矩形中心。
   - 黄色预测目标仍使用预测坐标；UART 控制点继续使用时序滤波结果。
2. 暴露并调整生产卡尔曼参数。
   - `TARGET_LOWPASS_ALPHA=0.75`。
   - 新增 `TARGET_KALMAN_PROCESS_NOISE=1000.0`、`TARGET_KALMAN_MEASUREMENT_NOISE=9.0`。
   - `TargetDetector` 将配置参数注入 `TargetTemporalProcessor`，不再依赖硬编码卡尔曼默认值。
3. 精简锁定画面叠加。
   - 新增 `DISPLAY_VERBOSE_STATUS=False`。
   - 锁定时只绘制目标/激光标记和 FPS；目标丢失时继续显示失败原因。
   - 详细目标、激光和 P95 文本仍可通过配置重新启用。
4. 继续优化 ROI 预处理。
   - `TARGET_REFINE_GAUSSIAN_SIZE` 从 5 改为 3。
   - 自适应兜底 `TARGET_REFINE_ADAPTIVE_BLOCK_SIZE` 从 31 改为 15。
   - 保留中值滤波和自适应阈值作为 Otsu 快速路径失败后的弱光/复杂光照兜底。
5. 未启用激光检测降频。
   - 30 FPS 下每两帧检测一次只有 15 FPS，并会在激光消失后继续使用约一帧旧坐标；现有测试要求消失帧立即返回无效，因此本次保留逐帧激光检测。

## 涉及文件

- `config.py`
- `drivers/display.py`
- `vision/target/detector.py`
- `vision/filters/target_temporal.py`
- `tests/test_display.py`
- `tests/test_target_temporal.py`

## 验证情况

- 30 FPS 滤波仿真：附件建议的 `alpha=0.65/Q=50/R=20` 在 10 px/帧匀速输入第 15 帧仍落后约 95 px，因此未采用。
- 当前 `alpha=0.75/Q=1000/R=9` 在 30 帧匀速测试末端滞后小于 8 px；静止 2 px 高斯测量噪声桌面仿真输出标准差约 1 px。
- 显示测试确认红色准星落在四角几何中心，精简锁定模式只绘制 FPS 文本。
- 320×240 合成锁定场景 `refine()` 桌面平均耗时约 229.1 µs；前一配置约 271.8 µs，最初实现约 702.9 µs。
- 已运行 `python -B -m unittest discover -s tests -v`：59 项测试通过。
- 未在 MaixCAM 实机验证真实 FPS、加速度响应、静态抖动和弱光检测率。

## 未处理事项

- 真实滤波参数仍需用设备上的静止靶、匀速移动、突然反向和短遮挡场景调校；桌面合成输入不能代表镜头噪声和手持运动。
- 若实机 FPS 仍低，需要新增 laser/display 分段计时后再决定激光算法优化，不能以旧激光坐标换取帧率而不做安全验收。
- 高斯核 3、自适应 block 15 在强光和阴影下的回退成功率需要实测；失败时可独立回退这些两个参数，不影响准星与卡尔曼调整。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\config.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\drivers\display.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\filters\kalman.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\filters\target_temporal.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\target\roi_refine.py`
- Tool: `functions.exec -> shell_command`，command `python -B -`（滤波响应、静态噪声和 ROI 微基准），cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
- Tool: `functions.exec -> shell_command`，command `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
