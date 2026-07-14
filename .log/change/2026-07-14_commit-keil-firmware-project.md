# 2026-07-14 提交 MSPM0 Keil 固件工程

## 修改目标

- 将桌面上的实际烧录工程源码和 Keil 工程配置纳入 `TrackingCar-Vision`，使远程仓库可查看 `main.c` 及其当前 UART 配置。

## 修改内容

1. 新增 `mspm0/firmware/`。
   - 包含 `main.c`、`gimbal_motor.*`、`empty.syscfg`、`ti_msp_dl_config.*`、视觉通信说明和 Keil 工程配置。
   - 不复制视觉通信源码；它与既有 `mspm0/vision_comm/` 内容逐文件一致，工程以相邻目录引用该模块。
2. 添加 Keil 构建和个人调试文件的忽略规则。
   - 排除 `Objects/`、`*.axf`、`*.hex`、`*.map`、`*.uvoptx` 和 `*.uvguix`。
3. 记录 TI SDK 原始 `source/` 依赖。
   - 原始目录约 155 MB、2171 个文件，未纳入仓库；使用者需从 MSPM0 SDK `2.05.01.00` 放置到 `mspm0/firmware/source/`。

## 涉及文件

- `C:\Users\Aupassen\Desktop\视觉\mspm0\firmware\`
- `C:\Users\Aupassen\Desktop\视觉\.gitignore`
- `C:\Users\Aupassen\Desktop\视觉\.log\change\2026-07-14_commit-keil-firmware-project.md`

## 验证情况

- 已逐文件对比 `mspm0/vision_comm/` 与 `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\`，内容一致。
- 已用 TI Arm Clang 对导入的 `main.c` 和 `gimbal_motor.c` 进行语法编译，两个编译返回码均为 0。
- 实机下载和 PA23 的周期帧仍需硬件验证。

## 未处理事项

- 未提交 TI SDK 的 `source/` 目录、Keil 构建产物或个人调试状态。
- 未改变电机协议、SysConfig 参数或视觉接口。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\keil\empty_LP_MSPM0G3507_nortos_keil.uvprojx`
- Tool: `functions.exec`，PowerShell `Copy-Item`、`git diff --no-index`，cwd `C:\Users\Aupassen\Desktop\视觉`
- Tool: `functions.exec`，`apply_patch`，cwd `C:\Users\Aupassen\Desktop\视觉`
