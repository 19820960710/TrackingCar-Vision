# 2026-07-14 添加 V2 全局重捕与降级状态机

## 修改目标

- 在小 ROI 精定位连续失败后清除错误 ROI，按 YOLO 全局重捕、低分辨率黑框粗提议、LOST 的顺序恢复或降级，并为每次状态变化保留原因、置信度和时间戳。

## 修改内容

1. 扩展状态机
   - `TRACK_ROI_REFINE`：仅使用当前 `active_roi` 调用传统精定位。
   - 连续失败达到 `ROI_FAIL_TO_GLOBAL_FRAMES` 后立即清空 `active_roi`，禁止错误区域继续锁定。
   - 模型可用时进入 `AI_GLOBAL_RECAPTURE`；AI 给出粗矩形后，同帧重新调用传统精定位。
   - 模型不可用、推理失败或未找到目标时，进入 `CLASSICAL_GLOBAL_PROPOSAL`。
   - 黑框粗提议成功后也必须经过传统精定位；精定位失败或无粗提议时进入 `LOST`。
2. 增加提议接口
   - `AiRecapture` 只包装外部提供的 YOLO 模型；仓库未捆绑模型资产时，明确返回 `MODEL_UNAVAILABLE`。
   - 黑框提议先缩放到低分辨率，寻找黑色连通矩形，输出粗 ROI，不直接当作有效靶标。
3. 增加状态证据
   - 每次 `step()` 返回目标量测和独立状态记录：`mode/reason/confidence/timestamp_ms/roi_fail_count/active_roi/transitioned`。
   - 失败量测仍使用统一数据契约，不携带历史四角或历史靶心。
4. 增加单元测试
   - 覆盖 ROI 连续失败后 AI 换新 ROI、模型不可用时黑框降级、全部失败进入 LOST、黑色矩形粗 ROI 与空白帧失败。

## 涉及文件

- `app/state_machine.py`
- `vision/target/detector.py`
- `vision/target/ai_recapture.py`
- `vision/target/global_proposal.py`
- `config.py`
- `tests/test_state_machine.py`
- `tests/test_global_proposal.py`
- `app.yaml`
- `.log/change/2026-07-14_add-v2-global-recovery.md`

## 验证情况

- 已运行状态机与黑框粗提议专项测试共 8 项，通过。
- 已对工程内 57 个 Python 文件执行内存语法编译，通过。
- 已执行 `python -B -m unittest discover -s tests -v`：30 项测试全部通过。
- 尚未在 MaixCAM Pro 上加载真实 YOLO 模型；模型输入尺寸、耗时、置信度门限和现场黑色背景误检率未验证。
- 尚未在真实靶纸连续移动场景下验证重捕成功率和从 LOST 恢复的时间。

## 未处理事项

- 外部 YOLO 模型必须实现 `model(frame) -> {valid, rect, confidence}` 契约后才能启用 AI 路径。
- 黑框粗提议仅是模型缺失时的低分辨率兜底，不取代传统精定位和后续几何验证。
- 本步骤不接入电机控制、激光识别或 UART 发送。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\state_machine.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\target\roi_refine.py`
- Source: `C:\Users\Lenovo\maixpy-official-reference\docs\doc\zh\vision\opencv.md`
- Tool: `functions.apply_patch` 生成状态机、粗提议和测试；测试命令 `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`。
