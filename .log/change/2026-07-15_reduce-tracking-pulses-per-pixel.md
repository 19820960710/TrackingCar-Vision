# 2026-07-15 降低视觉误差到电机脉冲的比例

## 修改目标

- 降低真实 MaixCAM 跟踪时每次视觉修正的转动角度，便于安全判断 Yaw、Pitch 的物理反馈方向。

## 修改内容

1. 将双轴共用的 commissioning 比例从 `8.0` 降为 `0.5` 脉冲/像素。
2. 暂不修改方向、死区、速度、加速度、命令周期和最大脉冲限制，保证本次实验只有一个控制变量。

## 涉及文件

- `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`

## 验证情况

- 静态计算：512×320 画面下，忽略死区时最大水平误差约对应 128 脉冲/次，最大垂直误差约对应 80 脉冲/次；均低于当前 400 脉冲上限。
- Keil Target 编译通过：`0 Error(s), 0 Warning(s)`；SysConfig 仍有一条既有的 Flash 状态最佳实践提示。
- 尚未实机验证方向；烧录后必须分别观察目标位于右侧、左侧、下方、上方时，下一帧误差绝对值是否减小。

## 未处理事项

- 如果物理反馈方向错误，较小比例只能减缓跑偏，不能阻止持续同向累计；确认方向后需单独修改对应轴的 `positive_error_is_cw`。
- 暂不调整死区和控制周期，避免与比例改动混淆。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source: 用户对真实相机接入后云台持续大角度转动的实机观察
- Tool: `functions.apply_patch`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
