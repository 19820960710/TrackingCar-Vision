# 2026-07-14 添加 V2 单应性、毫米坐标与画圆模块

## 修改目标

- 基于实测白色内框尺寸，将靶框四角映射到标准毫米平面，支持激光/靶心像素坐标映射、60 mm 圆轨迹反投影和稳定靶框圆点缓存；禁止用推导尺寸制造毫米结果。

## 修改内容

1. 新增实测尺寸契约
   - 新增 `PLANE_WHITE_WIDTH_MM/PLANE_WHITE_HEIGHT_MM/PLANE_OUTER_WIDTH_MM/PLANE_OUTER_HEIGHT_MM`。
   - 默认均为 `None`；调用 `MeasuredTargetDimensions.from_settings()` 时会拒绝未填写的白框尺寸。
   - 原有 `TARGET_WHITE_*` 保留为传统视觉长宽比的临时参数，明确不作为毫米标定值。
2. 新增标准平面映射
   - 白色内框按 `左上(0,0)、右上(宽,0)、右下(宽,高)、左下(0,高)` 建立平面。
   - 使用现有纯 Python 单应矩阵完成像素→mm 与 mm→像素映射；支持目标/激光量测坐标转换。
3. 新增圆轨迹与缓存
   - 以白框几何中心生成 50 个、半径 60 mm 的标准平面圆点，并逆投影回原图。
   - 靶框四角每点移动不超过 0.75 px 时复用已缓存圆点；超过阈值才重新计算矩阵和圆点。
4. 新增实机验收文档与测试
   - 文档要求先尺量白框/外框，再验证两点距离和圆轨迹最大误差。
   - 固定输入测试覆盖四角/靶心、像素毫米往返、激光量测、圆半径误差和缓存复用。

## 涉及文件

- `vision/geometry/standard_plane.py`
- `vision/geometry/circle_path.py`
- `vision/geometry/__init__.py`
- `config.py`
- `docs/plane_measurement.md`
- `tests/test_standard_plane.py`
- `app.yaml`
- `.log/change/2026-07-14_add-v2-standard-plane.md`

## 验证情况

- 已运行标准平面专项测试 4 项，通过。
- 已对工程内 62 个 Python 文件执行内存语法编译，通过。
- 已执行 `python -B -m unittest discover -s tests -v`：42 项测试全部通过。
- 尚未取得实际成品靶纸的尺量数据，故 `PLANE_*` 标定字段未填写，未在 MaixCAM Pro 上输出毫米坐标。
- 尚未用尺验证实际两点距离、60 mm 圆轨迹或透视边缘误差。

## 未处理事项

- 先测量并填写成品白色内框和外框尺寸；若误差异常，应复测尺寸、四角顺序和纸张平整度，不应调滤波掩盖比例错误。
- 需要在最终相机安装高度、靶纸距离和锁定曝光下记录实际误差。
- 本步骤不修改电机控制、UART 协议或激光阈值。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\vision\geometry\homography.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\docs\interface_contract.md`
- Tool: `functions.apply_patch` 生成标准平面、文档和测试；测试命令 `python -B -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`。
