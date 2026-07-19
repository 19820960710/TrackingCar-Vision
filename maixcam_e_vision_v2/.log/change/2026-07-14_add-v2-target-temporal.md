# 2026-07-14 添加 V2 目标状态确认、滤波与预测

## 修改目标

- 在目标模块输出后进行合法性检查、异常点门控、连续帧确认、一阶低通与可选常速度卡尔曼预测；在短遮挡期间避免跳变，在预测限制外清空旧坐标。

## 修改内容

1. 实现目标时序处理器
   - 合法目标必须具有有限靶心坐标和凸四边形角点。
   - 跳变门限取 `max(12 px, 0.30 × 当前靶框最大边长)`；异常帧不更新真实量测基准。
   - 首次捕获需要连续 3 帧，预测到期后的再次捕获需要连续 2 帧。
2. 实现低通与预测
   - 对真实量测先进行一阶低通，再可选使用二维常速度卡尔曼滤波。
   - 仅在最后一次真实量测后的 80 ms、且不超过 5 帧内输出 `predicted=True`。
   - 预测到期时返回 `valid=False`、坐标 `(0,0)`、角点 `None`，清除历史点和滤波器状态，避免远距离旧点被异常门控锁住。
3. 接入目标检测入口
   - 状态机先处理当前帧靶标；时序处理器再生成面向下游协议的统一量测。
   - 预测帧包含当前 `age_ms`；MSPM0 仍必须以 100 ms 看门狗实施最终安全停止。
4. 增加测试
   - 覆盖三帧首次确认、异常点与遮挡预测、5 帧预测上限、预测到期清空旧坐标、两帧重新确认。

## 涉及文件

- `vision/filters/target_temporal.py`
- `vision/filters/kalman.py`
- `vision/filters/__init__.py`
- `vision/target/detector.py`
- `config.py`
- `tests/test_target_temporal.py`
- `app.yaml`
- `.log/change/2026-07-14_add-v2-target-temporal.md`

## 验证情况

- 已运行目标时序专项测试 4 项，通过。
- 已对工程内 59 个 Python 文件执行内存语法编译，通过。
- 已执行 `python -B -m unittest discover -s tests -v`：34 项测试全部通过。
- 尚未在 MaixCAM Pro 实机测量滤波前后角点抖动、遮挡时长、卡尔曼参数与真实追踪延迟。
- MSPM0 100 ms 看门狗属于主控端责任，本步骤未修改 MSPM0 工程。

## 未处理事项

- 低通系数、过程噪声、量测噪声和跳变门限是初始参数，需按最终靶框像素尺寸、帧率和电机响应实测调整。
- `predicted=True` 仅在 80 ms/5 帧窗口内有效；如果主控策略不允许预测控制，应以该字段拒绝预测帧。
- 本步骤不增加激光滤波和电机控制。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\protocol\safety.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\models.py`
- Tool: `functions.apply_patch` 生成时序模块和测试；测试命令 `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`。
