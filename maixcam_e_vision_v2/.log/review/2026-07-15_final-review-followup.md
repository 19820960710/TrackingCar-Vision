# 2026-07-15 final-review 实机数据续审

## 检查范围

- `.log/review/2026-07-15_final-review.md` 的 A1-A16、低优项和验证结论。
- 用户提供的 MaixCAM Pro 实机 capture、target、laser、display、frame_cpu、frame_total P95。
- 当前配置、ROI 精炼、全局黑框提议、激光检测和显示实现。

## 检查依据

- 用户约束：不得削弱其它完好功能。
- 320×240 最终显示目标约 30 FPS，即整帧预算约 33.3 ms。
- 当前 ASCII 量测 100 ms 新鲜度和激光消失当前帧立即无效测试。
- MaixPy 官方相机/显示 API 文档。

## 发现

1. 审查中的 A15 激光降频不再采纳，且已从当前代码移除。
   - 影响: 原实现每两帧检测一次，并在跳过帧原样返回旧结果；旧结果仍携带 `updated=True`、`age_ms=0`，目标若在跳过帧失效也可能先返回缓存激光。
   - 证据: final-review 对 `_frame_counter`、`_laser_every_n=2`、`_last_laser_result` 的描述；当前 `test_disappearance_returns_invalid_not_old_laser_point`。
   - 处理: 恢复每帧激光检测，只缓存固定 LAB 阈值数组。实机激光 P95 约 3-6 ms，不是 75-82 ms 慢帧主因。
2. A1 的 320×240@30 已过时。
   - 影响: 30 Hz 传感器采集不给串行处理留下帧间余量。
   - 证据: 当前启动日志为 `camera active: 320x240 @ 60.0 fps, buffers=1`，capture P95 已降到约 2-7 ms。
   - 处理: 保持 320×240@60 采集，最终算法和显示仍以完整逐帧方式运行。
3. 实机稳定锁定瓶颈为 target 与 display 叠加。
   - 影响: target 约 16-21 ms、laser 约 3-6 ms、display 约 19-21 ms，串行总计约 44-50 ms，只能得到约 20-23 FPS。
   - 证据: 用户提供的多组 P95 日志。
   - 处理: 本轮优化目标 Otsu 热路径；显示先暂停 MaixVision 图像预览重新测量，避免把预览压缩/传输算入物理屏幕成本。
4. 75-82 ms target 慢帧来自丢失态全分辨率全局搜索。
   - 影响: 无目标或重捕期间整帧总耗时约 99-108 ms。
   - 证据: 激光在这些帧仅约 0.2 ms，target 与 frame_cpu 同时上升到约 76-82 ms。
   - 处理: 全局黑框改为直接 BGR 黑色掩码，保留全分辨率和远距离能力。
5. final-review 保留 adaptiveThreshold 的决定继续采纳。
   - 影响: 删除兜底可能在复杂光照下丢失目标。
   - 证据: 当前没有实机 Otsu 回退比例；final-review 也要求先测量。
   - 处理: 清晰 Otsu 跳过 Gaussian；失败后原 Gaussian+adaptive 继续执行。
6. 去除 flood-fill 的建议不成立。
   - 影响: 直接 `RETR_LIST` + 边界过滤会选到黑框外边界，使角点向外偏移约一个边框厚度。
   - 证据: 12 组合成对照中有效性相近，但角点由白色内框移动到黑色外框；桌面代表样例还更慢。
   - 处理: 不采纳；仅复用 flood-fill 掩码。
7. 卡尔曼、低通和二进制协议的低优结论不因本轮 FPS 数据改变。
   - 影响: 它们不是当前 target 预处理或 display P95 的主因。
   - 证据: target P95 随 TRACK/全局搜索路径变化，显示/控制坐标已分离，二进制未启用。
   - 处理: 本轮不修改。

## 已采纳

- 采纳 A16 的模块级 OpenCV/NumPy 缓存方向，并扩展到固定 LAB 阈值、ROI 和全局配置缓存。
- 采纳保留 adaptiveThreshold 兜底的决定。
- 采纳显示/控制坐标分离、Q=1000 有意调参和 ASCII 协议暂不变的结论。
- 根据实机 P95 新增 Otsu、全局黑色掩码、flood mask 和重复角点排序优化。

## 未采纳

- 不采纳 A15 激光降频；原因是它复用旧量测、削弱当前帧安全语义，且实测并非主要慢帧来源。
- 不采纳完全删除 adaptiveThreshold；原因是缺少复杂光照回退率数据。
- 不采纳直接轮廓过滤替代 flood-fill；原因是角点语义从白色内框变成黑色外框且性能未改善。
- 不采纳无文档依据的异步 `Display.show()`；原因是官方 API 无非阻塞参数，跨线程相机帧生命周期未验证。

## 验证情况

- 300 个合成噪声/亮度/斜视样例：直接 Otsu 与 Gaussian+Otsu 均 300/300 成功，角点差 P95 为 2 px。
- 新增快速路径与深色彩块拒绝测试。
- 29 项相关测试通过；66 项全量测试通过。
- 修改后实机 FPS、物理显示耗时和 Otsu 回退率仍需要重新运行测量。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-review-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_final-review.md`
- Source: 用户本轮提供的 MaixCAM Pro P95 日志。
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\target\roi_refine.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\target\global_proposal.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\laser\detector.py`
- Source: `https://en.wiki.sipeed.com/maixpy/doc/en/vision/display.html`
- Tool: `functions.exec -> shell_command`，commands 为源码复核、合成对照、微基准和 `python -B -m unittest discover -s tests`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
- Tool: `web.run`，仅查询 Sipeed/MaixPy 官方显示文档。