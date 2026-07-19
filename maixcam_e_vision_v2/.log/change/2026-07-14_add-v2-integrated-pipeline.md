# 2026-07-14 添加 V2 总流水线集成与性能调度

## 修改目标

- 由 `app/pipeline.py` 统一调度目标状态机/滤波、激光检测、可选毫米几何、ASCII UART 输出和分阶段性能统计；提供 MaixCAM Pro 实机性能与端到端记录工具。

## 修改内容

1. 集成每帧流水线
   - 运行时抓帧后调用目标检测器；目标检测器内部完成全局/ROI 状态机、传统精定位和目标时序处理。
   - 目标量测后调用激光检测器；激光模块自身执行确认、背景排除和跳变重锁。
   - 已填写实测平面尺寸且请求几何时，更新单应性、输出靶心/激光毫米坐标并按需提供缓存圆点。
   - 使用现有 ASCII 编码器写 UART；写失败仅记录失败，不伪造写入成功。
   - 目标或激光模块抛出异常时，转换为当前帧无效量测，继续产生安全 UART 帧，不让主循环直接终止。
2. 新增性能统计
   - 分别记录 `capture/target/ai_recapture/laser/geometry/uart_encode/uart_write/frame_cpu` 的滚动 P50/P95。
   - AI 计时仅包裹 AI 重捕适配器的 `detect()`，不与 ROI 精定位混合。
   - 设定稳定跟踪目标阶段 P95 预算 `12000 us`，但未声明实机达标。
3. 新增硬件运行与基准工具
   - `main.py` 创建相机、UART、目标、激光和流水线；二进制协议未验收时拒绝启动。
   - `tools/pipeline_benchmark.py` 连续运行 300 帧并输出各阶段 P95。
   - 新增中文记录表，要求由 MSPM0 记录完整 AIM 收帧时间，得到端到端延迟和 UART/解析延迟。
4. 增加无硬件测试
   - 覆盖抓帧计时、目标→激光→几何→UART 顺序、UART 写入、AI 计时字段、性能快照和模块异常降级为安全无效帧。

## 涉及文件

- `app/timing.py`
- `app/runtime.py`
- `app/pipeline.py`
- `vision/target/detector.py`
- `main.py`
- `tools/pipeline_benchmark.py`
- `docs/pipeline_benchmark.md`
- `config.py`
- `tests/test_pipeline.py`
- `tests/test_pipeline_safe.py`
- `app.yaml`
- `.log/change/2026-07-14_add-v2-integrated-pipeline.md`

## 验证情况

- 已运行流水线专项测试 3 项，通过。
- 已对工程内 68 个 Python 文件执行内存语法编译，通过。
- 已执行 `python -B -m unittest discover -s tests -v`：48 项测试全部通过。
- 尚未在 MaixCAM Pro 运行 300 帧基准；稳定 ROI `target P95 < 12000 us` 未验证。
- 尚未取得端到端延迟、AI 重捕耗时或 MSPM0 完整 UART 收帧时间的实机记录。

## 未处理事项

- 必须在实机锁相机参数、完成白框尺量与激光背景标定后运行 `tools/pipeline_benchmark.py`，填写文档表格。
- 单向 A16 接线下，MSPM0 收帧时刻只能由 MSPM0 自己记录；MaixCAM 不能伪造该数据。
- 二进制协议仍未通过 MSPM0 固件验收，主运行入口只允许 ASCII AIM。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\app\pipeline.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\tools\hardware_baseline.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\docs\binary_protocol.md`
- Tool: `functions.apply_patch` 生成流水线、性能工具和测试；测试命令 `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`。
