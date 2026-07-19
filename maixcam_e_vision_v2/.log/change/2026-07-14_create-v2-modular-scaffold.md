# 2026-07-14 创建 V2 模块化视觉仓库骨架

## 修改目标

- 在独立桌面目录创建 MaixCAM Pro 视觉模块 V2 骨架，冻结旧版 `C:\Users\Lenovo\TrackingCar-Vision`，为后续限时比赛复用目标识别、几何、激光、滤波和协议模块建立边界。

## 修改内容

1. 创建独立仓库结构
   - 新根目录为 `D:\Documents\Desktop\MaixCAM-E-Vision-V2`。
   - 建立 `app`、`drivers`、`vision`、`protocol`、`tests`、`tools`、`docs`、`dist` 和 `.log` 目录结构。
2. 建立可复用模块边界
   - `app/models.py` 定义字典化量测契约；目标和激光均具有 `valid`、`updated`、`predicted`、`lost_hold`、时间戳、年龄和置信度字段。
   - `vision` 只处理识别、几何和滤波；`drivers` 只访问相机和 UART；`protocol` 只负责编码；`app/pipeline.py` 只负责调度。
3. 固化硬件约束
   - 配置 MaixCAM UART0：A16/UART0_TX、A17/UART0_RX、`/dev/ttyS0`、115200。
   - UART 初始化要求系统配置 `maix_comm_method=none`，避免系统协议与视觉数据混用。
   - 首版保留 ASCII `AIM,...` 编码器，二进制 CRC 协议仅建立边界，未启用。
4. 建立首批纯逻辑测试
   - 覆盖对角线交点、凸四边形、ASCII AIM 编码、CRC-16/IBM 和目标状态机转换。
5. 保持算法实现延后
   - YOLO 重捕、ROI 自适应二值化、洪泛填充、轮廓精定位、完整激光检测、单应性和卡尔曼均保留明确模块入口，未伪装为已完成能力。

## 涉及文件

- `D:\Documents\Desktop\MaixCAM-E-Vision-V2\`
- `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\change\2026-07-14_create-v2-modular-scaffold.md`

## 验证情况

- 对仓库中 45 个 Python 文件执行内存语法编译，全部通过。
- 执行 `python -m unittest discover -s tests -v`，7 个纯逻辑测试全部通过。
- 未在 MaixCAM Pro、MSPM0G3507 或张大头电机上运行；相机、UART 和算法模块尚未进行实机验证。
- 未初始化、提交或上传 GitHub 远端。

## 未处理事项

- 新仓库尚未实现完整目标、激光、单应性、AI、二进制协议或控制算法；下一步应按 V2 指导文档先做相机/UART0 最小链路和实机基准。
- `app.yaml` 为骨架元数据，尚未生成可安装包。
- 旧版 `C:\Users\Lenovo\TrackingCar-Vision` 仅作只读参考，后续 V2 变更不得写入该目录。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `D:\Documents\Desktop\MaixPy官方E题_最终方案选型.md`
- Source: `C:\Users\Lenovo\TrackingCar-Vision\maixcam\config.py`
- Source: `C:\Users\Lenovo\TrackingCar-Vision\maixcam\drivers\uart_output.py`
- Tool: `functions.apply_patch`，创建 V2 模块骨架和本日志，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
- Tool: `functions.shell_command`，执行内存语法编译和 `python -m unittest discover -s tests -v`，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`
