# 2026-07-15 提升 30 FPS 余量与两米级远距离首捕

## 修改目标

- 处理实机处理帧率约 24 FPS、无法稳定接近 30 FPS 的问题。
- 扩大首次全局搜索距离，使画面中约 12×18 至 30×45 像素的小黑框能够进入原有三帧确认链路。
- 不以降低目标、激光、红框或显示更新频率换取帧率，不复用旧激光量测。

## 修改内容

1. 将相机采集配置从 320×240@30 调整为 320×240@60。
   - 最终处理循环仍逐帧执行目标、激光、UART 和显示。
   - 60 Hz 只用于缩短等待下一张传感器帧的时间，为 30 FPS 完整处理留余量。
   - 保持 `CAMERA_BUFFER_NUM=1`，不通过增加缓存换吞吐，以免进一步增加跟踪延迟。
2. 扩大远距离全局首捕能力。
   - `GLOBAL_PROPOSAL_WIDTH` 从 160 改为 320，避免远处细黑框在缩小后只剩少量像素。
   - 黑色阈值从 70 改为 100，最小轮廓面积比例从 3% 改为 0.3%。
   - 加入宽高比误差上限 0.55，并按面积、宽高比和矩形度综合排序，降低放宽面积后选中极端扁宽暗块的风险。
   - 全尺寸搜索时跳过同尺寸 `resize`，固定依赖和配置改为模块级缓存。
3. 允许 ROI 精炼接受远距离小白区。
   - 相对面积下限从 5% 降为 1%，同时增加 64 像素绝对面积下限。
   - 保留凸四边形、宽高比、角度和连续三帧确认规则。
4. 恢复并固定每帧激光检测。
   - 移除现有每两帧检测一次并原样返回旧结果的逻辑。
   - 保留每帧 BGR→LAB、候选提取、消失判断和跳变确认。
   - 仅缓存不变的 LAB 上下界 NumPy 数组，避免每帧重复分配，检测结果等价。
5. 优化不改变结果的 Python 热点。
   - ROI 精炼缓存 OpenCV、NumPy 和固定配置字典；仅测试覆盖参数时复制字典。
   - `PerformanceStats` 用固定容量环形覆盖替代列表头部 `pop(0)`，P50/P95 样本集合语义不变。
   - 显示颜色模块首次使用后缓存，不再逐帧重复导入查找。
6. 增加实机分段诊断。
   - 启动时打印相机实际宽、高、FPS 和缓冲数，确认设备是否接受 60 Hz。
   - 每 60 帧打印 capture、target、laser、display、frame_cpu、frame_total 六段 P95。
7. 增加回归测试。
   - 16×24 像素黑框经过原有三帧确认后锁定。
   - 极端扁宽暗块被宽高比门控拒绝。
   - 生产相机配置固定为已声明的 320×240@60 候选。
   - 激光消失当前帧立即无效、显示/整帧计时继续覆盖。

## 涉及文件

- `config.py`
- `vision/target/global_proposal.py`
- `vision/target/roi_refine.py`
- `vision/laser/detector.py`
- `app/timing.py`
- `app/runtime.py`
- `drivers/display.py`
- `main.py`
- `tests/test_global_proposal.py`
- `tests/test_hardware_baseline.py`
- `tests/test_pipeline.py`

## 验证情况

- 修改前合成复现：12×18、16×24、20×30、24×36、30×45 像素黑框均无法首次提议，约 40×60 像素才成功。
- 修改后合成矩阵：12×18、16×24、20×30、24×36、30×45、40×60 像素均能首次提议，并在第三个连续当前帧后锁定。
- 极端扁宽暗块拒绝测试通过。
- 激光当前帧消失即无效、跳变二帧确认和背景排除测试通过。
- 相关测试 20 项通过；全量 `python -B -m unittest discover -s tests` 共 63 项通过，耗时约 0.110 s。
- 67 个 Python 文件内存编译通过，`app.yaml` 无缺失条目。
- 未执行 MaixCAM 实机 FPS 和两米距离测试；桌面合成像素尺寸不能证明真实镜头、打印质量、光照和对焦条件。

## 未处理事项

- 未降低激光、目标、红框或显示更新频率；没有使用旧激光点填充跳过帧。
- 未提高 `CAMERA_BUFFER_NUM`；更多缓冲可能提高读帧吞吐，但会增加控制图像时延。
- 当前 `PREDICT_HOLD_MS=120` 是本轮开始前已存在的值，本轮未修改。
- 60 Hz 与 30 Hz 使用的 ISP 配置可能产生轻微画面偏移或画质差异；需要在实机重新确认黑框阈值、激光 LAB 阈值和几何标定。
- 若实机仍低于 30 FPS，应依据新增 P95 判断是 capture、target、laser 还是 display 超时，不能继续无证据地删除处理步骤。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_v2-optimization-review.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\config.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\target\global_proposal.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\laser\detector.py`
- Source: `https://en.wiki.sipeed.com/maixpy/doc/en/vision/camera.html`
- Source: `https://wiki.sipeed.com/maixpy/api/maix/camera.html`
- Tool: `functions.exec -> shell_command`，commands 为合成目标矩阵、内存编译和 `python -B -m unittest discover -s tests`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
- Tool: `web.run`，仅查询 Sipeed/MaixPy 官方相机文档。