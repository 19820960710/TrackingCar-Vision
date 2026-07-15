# 2026-07-14 双轴视觉闭环控制接入

## 修改目标

- 将 MaixCAM 成功解析的目标图像坐标转换为 yaw、pitch 两路独立的增量位置命令，实现初始双轴视觉跟踪闭环。

## 修改内容

1. 按当前工程映射指定 PB6/PB7 通道为 yaw，PA23/PA24 通道为 pitch。
2. 新增 `g_yaw_tracker`，复用既有增量控制器；`g_yaw_tracker` 绑定 PB6 电机，已有 `g_pitch_tracker` 绑定 PA23 电机。
3. 每个有效观测帧计算 `dx = target_x - frame_width/2`、`dy = target_y - frame_height/2`。
4. `dx` 调用 yaw 控制器，`dy` 调用 pitch 控制器。两个控制器各自维护死区、40 ms 最短命令周期和最大脉冲限制。
5. 新增 yaw/pitch 正误差方向宏，初值均为 `true`；若实机方向相反，仅修改对应宏，不改控制算法。
6. 闭环控制仍在视觉 UART 延后启用之后运行，因此不会干扰开机双电机自检。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 使用 TI Arm Clang 编译主程序、生成配置、步进电机和视觉模块源文件，编译通过。
- 未实机验证。第一次测试应使目标小幅偏离画面中心，确认 PB6 仅响应水平 `dx`、PA23 仅响应垂直 `dy`；若某轴方向相反，翻转该轴 `*_POSITIVE_ERROR_IS_CW` 宏。

## 未处理事项

- 初始参数为每像素 0.25 脉冲、8 像素死区、每帧最大 30 脉冲、60 RPM、40 ms 限速，需按实际视场角和机械传动比调参。
- 当前使用目标相对画面中心的位置追踪，不使用 AIM 中目标相对激光的 `aim_dx/aim_dy`。
- `PITCH_MOTOR_COMMISSIONING_TEST_ENABLED` 仍为 `1U`，每次上电仍先完成自检后才进入跟踪。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_packet.c`
- Tool: `functions.exec`，命令 `C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe ...`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
