# 2026-07-14 添加 V2 纯几何模块

## 修改目标

- 为后续靶框识别提供不依赖相机、UART、OpenCV 和电机控制的几何基础：四角排序、四边形合法性、尺寸特征、靶心、ROI 裁剪、单应变换和坐标投影。

## 修改内容

1. 完善四边形工具
   - 输入四个任意顺序的点，输出图像坐标系中的 `左上、右上、右下、左下` 顺序。
   - 拒绝凹四边形、共线点、重复点、非有限数值和其他退化输入。
   - 输出四边长、四个内角与两组对边长度比。
2. 完善靶心和 ROI 工具
   - 对角线交点用于计算靶心。
   - ROI 根据图像边界裁剪；无正面积交集时返回 `None`。
3. 实现纯 Python 单应性工具
   - 通过带主元的高斯-约旦消元计算四点单应矩阵。
   - 支持单点投影和 3×3 矩阵求逆，不引入 OpenCV 或 NumPy 运行时依赖。
4. 增加固定输入测试和打包清单
   - 覆盖乱序四角、靶心、凹/共线/重复非法四边形、边角特征、ROI、单应正向与反向投影。
   - 将 `vision/geometry/roi.py` 加入 MaixVision 应用清单。

## 涉及文件

- `vision/geometry/quadrilateral.py`
- `vision/geometry/center.py`
- `vision/geometry/roi.py`
- `vision/geometry/homography.py`
- `vision/geometry/__init__.py`
- `tests/test_geometry.py`
- `app.yaml`
- `.log/change/2026-07-14_add-v2-pure-geometry.md`

## 验证情况

- 已对工程内 54 个 Python 文件执行内存语法编译，通过。
- 已执行 `python -B -m unittest discover -s tests -v`：22 项单元测试全部通过，其中几何模块 6 项。
- 本模块为纯数学代码，不需要 MaixCAM Pro 或 MSPM0 实机验证。

## 未处理事项

- 本步骤不包含图像中找轮廓、靶框筛选、物理尺寸标定或实际单应性误差评估；这些必须在获得实测靶纸尺寸后再接入视觉流程。
- 单应矩阵会拒绝退化点集；调用方需要将异常转为无效量测，而不是继续使用旧坐标。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\geometry\quadrilateral.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\tests\test_geometry.py`
- Tool: `functions.apply_patch` 生成模块、测试、应用清单和日志；测试命令 `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`。
