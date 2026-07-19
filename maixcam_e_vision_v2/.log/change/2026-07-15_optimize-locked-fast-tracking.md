# 2026-07-15 优化锁定帧率与快速移动跟踪

## 修改目标

- 处理目标锁定后 FPS 降到 10 以下，以及快速移动时目标框容易丢失的问题。

## 修改内容

1. 优化 ROI 白区边界清理。
   - 旧实现沿 ROI 四边逐点检查，并可能执行数百次 `cv2.floodFill`。
   - 新实现给二值图增加一圈白色边框，只执行一次 flood fill，再裁掉边框；100 组随机二值图与旧实现结果一致。
2. 增加快速阈值路径并保留原算法兜底。
   - 先对高斯滤波灰度图执行 Otsu 阈值；若找到合法四边形，跳过中值滤波和自适应阈值。
   - 快速路径未找到合法目标时，继续执行原有中值滤波和自适应阈值，保留复杂光照适应性。
3. 扩大快速运动跟踪范围。
   - `ROI_MARGIN_MIN_PX` 从 20 改为 32。
   - `ROI_MARGIN_RATIO` 从 0.25 改为 0.35。
   - `TARGET_OUTLIER_JUMP_MIN_PX` 从 12 改为 28。
   - `TARGET_OUTLIER_JUMP_RATIO` 从 0.30 改为 0.50。
4. 修正跳变门控参照点。
   - 使用上一次原始检测点判断跳变，不再与滞后的滤波坐标比较，避免正常快速移动被滤波延迟误判为异常。
5. 增加预测状态守卫。
   - 内部状态不一致、预测点仍为 `None` 时安全失效并清空预测状态，不再访问 `point[0]` 引发异常。
6. 增加快速路径、回退路径、快速移动和内部状态异常测试。

## 涉及文件

- `vision/target/roi_refine.py`
- `vision/filters/target_temporal.py`
- `config.py`
- `tests/test_roi_refine.py`
- `tests/test_target_temporal.py`

## 验证情况

- 单次 flood fill 等价性：100 组随机二值图与旧实现一致；该步骤桌面微基准从约 130.8 µs 降至 18.5 µs，约 7.1 倍。
- 相同 320×240 合成锁定场景的 `refine()` 桌面平均耗时：改前约 702.9 µs，最终约 271.8 µs，减少约 61.3%。
- 快速 Otsu 路径和强制自适应回退路径测试均通过。
- 快速一致移动测试通过：确认后单帧移动 24 像素仍输出当前帧更新结果，不进入预测。
- 已运行 `python -B -m unittest discover -s tests -v`：57 项测试通过。
- 未在 MaixCAM 实机测量；桌面 OpenCV 数字不能直接换算为设备 FPS。

## 未处理事项

- 激光 LAB 检测仍每帧运行。若实机 `P95 T` 已下降但 `P95 F` 仍高，需要继续单独测量激光检测和 `screen.show()`，再决定是否优化颜色转换或显示路径。
- 扩大 ROI 会增加精炼像素量，但用于容纳快速帧间位移；需在实机同时观察 FPS 与移动丢框率，不能只看静态场景。
- 放宽跳变门限可能提高错误候选被接受的概率；当前仍保留合法四边形、ROI、连续确认和预测安全规则，强光/复杂背景仍需实测。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\target\roi_refine.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\filters\target_temporal.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\target\roi.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\config.py`
- Tool: `functions.exec -> shell_command`，command `python -B -m unittest tests.test_roi_refine tests.test_target_temporal -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
- Tool: `functions.exec -> shell_command`，command `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
- Tool: `functions.exec -> shell_command`，command `python -B -`（OpenCV 等价性与微基准），cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
