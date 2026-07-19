# 云台 PD 自动调参

该工具只优化矩形中心回到画面中心时的 yaw/pitch `Kp`、`Kd`。它不评价激光落点，也不补偿摄像头与激光笔的视差。

## 安全条件

- 小车静止，标靶固定且在相机中持续可见。
- yaw、pitch 两轴均留有至少 320/240 相对脉冲的双向安全行程。
- 运行期间不要触碰云台；异常时先断步进电机电源。

## 使用

先通过 Keil 完成重建和烧录，再运行：

```powershell
python tools/gimbal_autotune/autotune.py --hardware --yes-clear-gimbal --skip-build-flash
```

离线检查邮箱布局和评分逻辑：

```powershell
python tools/gimbal_autotune/autotune.py --self-test
```

指定参数只做双种子复测：

```powershell
python tools/gimbal_autotune/autotune.py --hardware --yes-clear-gimbal `
  --skip-build-flash --validate-only `
  --yaw-kp 0.20 --yaw-kd 0.008 --pitch-kp 0.40 --pitch-kd 0.005
```

结果写入 `gimbal-autotune-results/<时间>/`。只有 `selected_candidate.json` 中 `accepted=true` 的参数才允许写回正式配置。
