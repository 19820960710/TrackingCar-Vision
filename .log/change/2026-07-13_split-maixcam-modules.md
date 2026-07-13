# 2026-07-13 拆分 MaixCam 主程序模块

## 修改目标

- 将 `maixcam/main.py` 中的设备访问、目标识别、激光识别、运行状态、绘图和帧流程拆分到独立模块，同时保持算法参数、判断分支、执行顺序和输出协议不变。

## 修改内容

1. 建立 MaixCam 分层目录。
   - `drivers/` 负责相机和 UART 设备访问。
   - `vision/` 负责几何工具、目标识别和激光识别。
   - `app/` 负责共享运行状态、调试绘图和逐帧处理流程。
2. 将原配置导入和 fallback 参数迁入 `maixcam/settings.py`。
   - `maixcam/config.py` 未修改。
   - 外部配置缺失时仍使用原 `fallback-main` 参数。
3. 将跨模块可写状态集中到 `maixcam/app/runtime_state.py`。
   - 调用方使用 `state.<name>` 访问，状态初始值未修改。
4. 将 `maixcam/main.py` 缩减为相机初始化、UART 初始化、显示初始化和主循环入口。
5. 调整 `.gitignore`，使 `.log/` 下的项目日志可以被 Git 跟踪。

## 涉及文件

- `C:\Users\Aupassen\Desktop\视觉\maixcam\main.py`
- `C:\Users\Aupassen\Desktop\视觉\maixcam\settings.py`
- `C:\Users\Aupassen\Desktop\视觉\maixcam\app\`
- `C:\Users\Aupassen\Desktop\视觉\maixcam\drivers\`
- `C:\Users\Aupassen\Desktop\视觉\maixcam\vision\`
- `C:\Users\Aupassen\Desktop\视觉\.gitignore`
- `C:\Users\Aupassen\Desktop\视觉\.log\change\2026-07-13_split-maixcam-modules.md`

## 验证情况

- `python -m compileall -q maixcam`：通过，拆分后的 Python 文件可编译。
- 使用临时 `maix` 模块桩依次导入设置、状态、几何、目标、激光、相机、UART、绘图、流水线和主入口：通过，未发现循环导入或缺失名称。
- UART 示例断言：通过；`AIM,1,-12,8,244,168,256,160,perspective,green` 和目标模式输出与拆分前格式一致。
- 函数清单与 AST 对比：原 `maixcam/main.py` 的 90 个函数均保留；移除 `global` 声明并归一化 `state.` 限定名后，函数体无差异。
- 配置 fallback 模拟导入：通过；`CONFIG_SOURCE`、图像尺寸和激光颜色仍使用原 fallback 值。
- `git diff --check`：通过。
- `git check-ignore -v --no-index .log/change/2026-07-13_split-maixcam-modules.md`：日志路径未被忽略。

## 未处理事项

- 未在 MaixCam Pro 真机运行。需要上传完整 `maixcam` 目录后实测相机启动、`CAL BASE`、`CAL TARGET`、目标红框、绿激光蓝点、FPS 和 UART 输出。
- 不再保证只上传一个 `maixcam/main.py` 即可运行；运行依赖同目录下的新模块和子目录。
- 未调整算法、阈值、未使用变量、重复分支、函数命名和现有文档。`docs/maixcam_quickstart.md` 的既有工作区状态未处理。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\.codex\skills\project-logbook\references\development-change-log.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\maixcam\main.py`
- Source: `C:\Users\Aupassen\Desktop\视觉\maixcam\config.py`
- Source: `C:\Users\Aupassen\Desktop\视觉\maixcam\README.md`
- Tool: `functions.shell_command`，command `python -m compileall -q maixcam`，cwd `C:\Users\Aupassen\Desktop\视觉`
- Tool: `functions.shell_command`，command `python -`（临时 Maix 模块桩、UART 断言、函数 AST 对比和 fallback 导入检查），cwd `C:\Users\Aupassen\Desktop\视觉`
- Tool: `functions.shell_command`，command `git diff --check`，cwd `C:\Users\Aupassen\Desktop\视觉`
- Tool: `functions.shell_command`，command `git check-ignore -v --no-index .log/change/2026-07-13_split-maixcam-modules.md`，cwd `C:\Users\Aupassen\Desktop\视觉`
- Tool: `functions.apply_patch`，用于创建拆分文件、调整 `.gitignore` 和写入本日志。
