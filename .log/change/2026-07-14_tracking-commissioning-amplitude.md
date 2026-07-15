# 2026-07-14 视觉闭环大幅度联调参数

## 修改目标

- 消除画面中心附近小误差被默认死区过滤的问题，并增大初始闭环命令幅度，便于肉眼确认 yaw/pitch 闭环是否已实际执行。

## 修改内容

1. 审查确认闭环链路为：完整帧解析 → `relay_latest_maixcam_observation()` 取帧 → 计算 `dx/dy` → 分别调用 PB6 yaw、PA23 pitch 的控制器。
2. 新增 `GIMBAL_TRACKING_COMMISSIONING_MODE`，当前为 `1U`。
3. 联调档将两个轴死区设为 0，每像素脉冲数设为 20，单次相对位置命令上限设为 800 脉冲。
4. 默认控制器参数未删除；将联调宏改为 `0U` 后恢复默认 8 像素死区、0.25 脉冲/像素、30 脉冲上限。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 使用 TI Arm Clang 编译主程序、生成配置、步进电机和视觉模块源文件，编译通过。
- 未实机验证。目标当前位于 `(255,159)`、画面中心 `(256,160)` 时，联调档也会每个控制周期产生可见的 20 脉冲级命令；目标偏离更大时单次最大可到 800 脉冲。

## 未处理事项

- 联调档命令幅度明显大于正式跟踪参数，应确保机械行程安全；确认闭环方向、轴映射后必须关闭该宏并重新调低参数。
- 若目标坐标持续不变但云台已机械到限位，控制器仍会持续尝试纠偏；当前没有加入限位开关或软件角度保护。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_packet.c`
- Tool: `functions.exec`，命令 `C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
