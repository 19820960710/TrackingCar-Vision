# 2026-07-15 根据实机 P95 优化目标热路径

## 修改目标

- 根据 MaixCAM Pro 实机分段 P95，降低锁定态约 16-21 ms 的目标检测耗时和丢失态约 75-82 ms 的全局搜索耗时。
- 保持目标、激光、红框、UART 和物理屏幕逐帧更新，不通过降频或复用旧量测换取 FPS。

## 修改内容

1. 优化 ROI 清晰帧快速路径。
   - Otsu 直接作用于灰度 ROI，不再在每个清晰帧前执行 3×3 GaussianBlur。
   - Otsu 找不到合法四边形时，仍执行原 GaussianBlur + adaptiveThreshold 兜底，复杂光照路径未删除。
2. 优化全局黑框预处理。
   - 使用 BGR 三通道 `inRange` 直接判断三个通道均低于黑色阈值的像素。
   - 删除全局路径中的 BGR→GRAY、GaussianBlur 和固定阈值组合。
   - 黑色语义更严格：单一颜色通道较亮的深色彩块不会被当成黑框。
3. 复用 flood-fill 掩码。
   - 仅在 ROI 尺寸变化时重新分配掩码；尺寸相同时清零复用。
   - 保留原单次 padded flood-fill 算法，没有改成会把角点移到黑色外框的轮廓边界方案。
4. 避免重复角点排序。
   - `_candidate()` 已输出有序角点，目标中心直接调用 `diagonal_intersection()`，不再第二次执行 `order_corners()`。
5. 增加回归测试。
   - 清晰 Otsu 路径禁止调用 GaussianBlur。
   - 全局黑框路径禁止调用灰度转换和 GaussianBlur。
   - 深色彩块不得被识别为黑框。
   - 保留远距离小框、adaptiveThreshold 回退、激光当前帧失效和目标时序测试。

## 涉及文件

- `vision/target/roi_refine.py`
- `vision/target/global_proposal.py`
- `tests/test_roi_refine.py`
- `tests/test_global_proposal.py`

## 验证情况

- 用户实机基线：稳定锁定时 target P95 约 16-21 ms、laser 约 3-6 ms、display 约 19-21 ms、frame_total 约 44-50 ms；丢失/全局搜索时 target P95 约 75-82 ms、frame_total 约 99-108 ms。
- 300 组合成噪声/亮度/斜视 ROI 对照：Gaussian+Otsu 与直接 Otsu 均 300/300 找到目标；配对角点最大差 2 px、P95 差 2 px。
- 桌面代表尺寸预处理微基准：Gaussian+Otsu 约 53.85 µs，直接 Otsu 约 18.10 µs；仅为相对数据。
- 全局黑色掩码桌面微基准：GRAY+Gaussian+threshold 约 59.70 µs，BGR inRange 约 47.19 µs；目标黑框像素量接近。
- 直接轮廓边界替代 flood-fill 的实验未采用：角点会从白色内框移动到黑色外框，且桌面代表样例约 58.11 µs，慢于 flood-fill 的 45.70 µs。
- 相关测试 29 项通过；全量 `python -B -m unittest discover -s tests` 共 66 项通过，耗时约 0.119 s。
- 未执行修改后的 MaixCAM 实机 P95；实际收益需重新部署后测量。

## 未处理事项

- 未删除 adaptiveThreshold 兜底；需要先获得 Otsu 回退比例和不同光照实测。
- 未异步化 `Display.show()`；官方 API 未提供非阻塞 show 参数，且跨线程复用相机帧存在缓冲区生命周期风险。
- 未降低物理屏幕刷新率。MaixVision 连接时 `Display.show()` 还会发送预览图，复测应暂停 MaixVision 图像预览以分离预览压缩/传输耗时。
- 若暂停预览后 display P95 仍约 20 ms，则稳定 30 FPS 还要求 target+laser+capture 总和低于约 13 ms，需要基于新一轮 P95 继续优化。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_final-review.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\target\roi_refine.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\target\global_proposal.py`
- Source: `https://en.wiki.sipeed.com/maixpy/doc/en/vision/display.html`
- Source: `https://wiki.sipeed.com/maixpy/api/maix/display.html`
- Tool: `functions.exec -> shell_command`，commands 为合成对照、微基准、语法检查和单元测试，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
- Tool: `web.run`，仅查询 Sipeed/MaixPy 官方显示 API 文档。