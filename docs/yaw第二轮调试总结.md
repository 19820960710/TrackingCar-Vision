# Yaw 角环第二轮调试总结

分支：`yaw-pid-debug-tuning-round2`
基线：`7913604 固化yaw抗扰PID参数`

## 现象与根因

用户反馈角度环：超调量大、抗干扰差、响应慢、调节时间长、存在静态误差；尤其是“阶跃或扰动后必须先静止一下才去修正稳态误差”。

通过 TEL 逐行采集（`err/ce/derr/boost/turn/wl/wr/l/r/lp/rp`）定位到三个叠加根因：

1. yaw 死区为硬切换：误差一进 1.5° 就 `turn=0`，瞬间失去修正力。
2. yaw 小误差段只给很小 `turn_rpm`，速度环要靠积分慢慢爬 PWM。
3. 速度环低速段推不动：yaw 给了 ±16RPM，但 `l/r=0`、PWM 只有 8~12，轮子克服不了静摩擦。

## 最终方案（已固化）

### yaw 层（位置式 PID + 非线性补偿）

默认参数：

```c
#define YAW_PID_DEFAULT_KP_MILLI    80
#define YAW_PID_DEFAULT_KI_MILLI    1
#define YAW_PID_DEFAULT_KD_MILLI    60
#define YAW_PID_OUTPUT_LIMIT_RPM    160

g_yaw_min_turn_rpm          = 15
g_yaw_deadband_deg10        = 15   // 1.5°
g_yaw_integral_zone_deg10   = 180  // 18° 内积分
g_yaw_integral_limit_rpm    = 6
g_yaw_min_turn_zone_deg10   = 180  // 18° 内启用连续恢复曲线
g_yaw_target_ramp_step_deg10 = 80  // 目标斜坡 8°/50ms
```

关键结构：

- **目标斜坡** `yaw_target_ramp_step`：目标不直接阶跃，按 8°/50ms 逼近，避免大角度阶跃惯性过冲。
- **动态输出限幅**：`dynamic_limit = 12 + abs_err/25`，大误差时限制最大转向速度。
- **软死区**：误差进 1.5° 内不直接归零，而是线性衰减到一个最小保持转向速度（约 4RPM），避免“刚到目标就失去修正力”。
- **连续恢复曲线** `yaw_recover_turn_for_error`：误差 1.5~18° 内，`turn` 在 15~30RPM 间连续变化，避免分段停顿；超过 18° 用 PID 正常输出。
- **最小速度补偿只在目标斜坡完成后启用**（`control_target == target`），避免阶跃过程中提前推过头。
- **积分分离 + 限幅**：只在 18° 内积分，积分贡献限 ±6RPM。

### 速度层（增量式 PID + 低速启动前馈）

```c
#define SPEED_START_FF_SETPOINT_RPM 60
#define SPEED_START_FF_ERR_RPM      2
g_speed_start_ff_pwm = 14   // 在线命令 FFS 可调
```

- **低速启动前馈** `speed_apply_start_feedforward`：
  - 只在 `|setpoint| ≤ 60RPM` 且速度误差 `> 2RPM` 时启用。
  - 把 PWM 至少顶到 ±14，让小目标能真正转起来，不再等积分慢慢爬。
- **关键：前馈由 yaw 层开关** `g_speed_start_ff_enable`：
  - yaw 误差 > 死区且 `turn≠0` 时才开。
  - 进入死区/到位后立刻关，避免“yaw 想停、速度环还在顶”造成末端抖动。

## 调试用串口命令（仅调试分支）

```
PIDY <kp> <ki> <kd>     yaw PID milli
OUTY <rpm>              yaw 输出限幅
MINY <rpm>              最小转向速度
DBY <deg10>             死区
IZONEY <deg10>          积分分离区
ILIMY <rpm>             积分限幅
ZONEY <deg10>           连续恢复区
RAMPY <deg10/50ms>      目标斜坡步长
FFS <pwm>               速度低速前馈 PWM
BASE / YAW / YAW10 / START / STOP / ESTOP / CLR / WHEEL / HELP
```

## 调试脚本

- `scripts/yaw_pid_tune.py`：阶跃序列自动测试，输出超调/t90/settle/boost 等指标。
- `scripts/yaw_disturb_collect.py`：0° 保持态扰动事件切片分析。
- `scripts/yaw_tel_trace.py`：逐行打印 `err/turn/wl/wr/l/r/lp/rp`，定位卡在哪一层。
- `scripts/yaw_speed_inner_check.py`：速度内环阶跃检查。
- `scripts/serial_send_cmd.py`：在线发单条命令。

## 实测结论

- 阶跃 0→90→-90→0：静差 0.5~3.2°，超调 0~5°，无 reach2=None。
- 扰动保持：回正连续，不再“停一下再修”；到位后末端不抖（FFS 被关）。
- 优先级符合用户要求：调节时间 > 稳态误差 > 超调量。

## 注意

- 180° 跨 ±180° 边界时如果 hold 太短会看起来异常，属于测试时段不足 + 边界跳变，非算法 bug。
- 生产版合并前需做“清理提交”：删除串口命令解析、TEL 高频遥测、调试脚本，只保留最终控制逻辑和默认参数。
