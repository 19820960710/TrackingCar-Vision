# 大小电机速度档案抽象

## 目标

将轮径、减速比、编码器口径、控制周期和 PID 从 MG680 专用宏提升为可选择
的电机档案，为未来小电机接入提供明确入口，同时防止误用大电机参数。

## 实施

- 新增 `motor_speed_profile_t`，封装身份、机械、编码器、周期、PID 和 PWM。
- 建立已验证 `g_mg680_large_motor_profile`。
- 建立未标定 `g_small_motor_template_profile`，默认不可启用。
- active profile 只有一个选择点，速度服务初始化时执行完整有效性检查。
- 控制核心的采样/控制周期改由 profile 注入，不再写死 10/30 ms。
- 删除 encoder 层残留的 440 CPR、30:1 和 RPM 换算；编码器层只负责 count。
- yaw 到速度环的单位换算改为使用 active profile 的轮径。

## 参数边界

- MG680 精确减速比尚未确认，记录为 0 且 `gear_ratio_verified=false`。
- MG680 左右输出轴 CPR 已实测，因此速度环仍可可靠换算。
- 小电机所有未知参数保持为 0，不能复制 MG680 参数后声称已标定。

## 验证

- Arm Compiler 6.24 全量构建通过：0 error，0 warning。
- 未烧录；本次配置抽象不扩大此前 MG680 实机验证范围。

## 使用的技能

- `c-style`：配置、控制、服务和驱动职责分离，单位通过字段名表达。
- `project-logbook`：记录未知参数、验证边界与未来接入步骤。
