# 2026-07-14 添加 V2 ROI 传统靶框精定位

## 修改目标

- 仅在调用方提供的当前 ROI 内寻找白色内框，输出当前帧的四角和靶心；任何失败都输出明确的无效量测，不能复用旧数据。

## 修改内容

1. 实现传统 ROI 图像处理链
   - 先裁剪 ROI，再交给 OpenCV，避免整帧转换。
   - 依次执行灰度、高斯滤波、中值滤波、自适应二值化、边界白色连通域 floodFill、外轮廓、四边形逼近与几何筛选。
2. 实现几何筛选和当前帧量测
   - 使用纯几何模块完成凸性、四角排序、边长比例、内角和对角线靶心计算。
   - 按白色内框标定长宽比、最小面积、角度范围和轮廓逼近误差筛选候选；分数最高者输出全图坐标。
3. 固化失败语义
   - 无 ROI、ROI 越界、CV 不可用、预处理失败、无合法四边形和靶心失败等情形，均返回新的 `valid=False` 量测。
   - 失败量测的 `updated/predicted/lost_hold` 均为 `False`，四角为 `None`，坐标为 `(0, 0)`；不保存也不返回任何旧靶标坐标。
4. 增加合成固定图像测试
   - 覆盖轻微斜视的大靶框、第二种缩放尺度和空白 ROI 失败语义。

## 涉及文件

- `config.py`
- `vision/target/roi_refine.py`
- `vision/target/detector.py`
- `tests/test_roi_refine.py`
- `app.yaml`
- `.log/change/2026-07-14_add-v2-roi-target-refine.md`

## 验证情况

- 已运行 ROI 精定位测试：固定斜视靶框、第二种缩放靶框、空白 ROI 共 3 项，全部通过。
- 已对工程内 55 个 Python 文件执行内存语法编译，通过。
- 已执行 `python -B -m unittest discover -s tests -v`：25 项测试全部通过。
- 尚未在 MaixCAM Pro 实机、真实靶纸、不同距离和现场光照下测量成功率、FPS 与角点误差。

## 未处理事项

- 标定白框尺寸目前仍使用配置中的 174.0 mm × 261.0 mm 起始值；必须实测最终靶纸白色内区后回填。
- OpenCV 在 MaixCAM 上为 CPU 计算。本模块先裁 ROI 以降低开销，但阈值、ROI 尺寸和帧率仍需实机基准后调整。
- 本步骤不负责全帧搜索、ROI 跟踪、连续帧确认、滤波或 UART 发送。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `C:\Users\Lenovo\maixpy-official-reference\docs\doc\zh\vision\opencv.md`
- Source: `C:\Users\Lenovo\maixpy-official-reference\docs\doc\zh\vision\image_ops.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\geometry\quadrilateral.py`
- Tool: `functions.apply_patch` 生成代码、测试和日志；测试命令 `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`。
