# V2 第 1 步接口契约

本文件是 V2 后续代码的固定边界。任何模块不得自行改变引脚、串口参数、字段语义或安全规则。确需修改时，必须先修改本文件、提高应用版本、补充测试并写 `.log/change/`。

## 1. 硬件接口

| 项目 | 固定值 | 规则 |
|---|---|---|
| MaixCAM 发送 | A16 / UART0_TX | 接 MSPM0 UART0_RX |
| 公共参考 | GND | 两端必须共地 |
| MaixCAM 接收 | A17 / UART0_RX | 默认不接、不映射；仅双向命令正式启用后使用 |
| 供电 | 各自独立供电 | 不连接扩展板 UART 的 +5 V/VBUS |
| 串口设备 | `/dev/ttyS0` | 固定使用 UART0 |
| 通信参数 | 115200、8N1、无流控 | 两端一致 |
| 系统设置 | `maix_comm_method=none` | 写入 `/boot/configs` 后必须重启 |

默认配置固定为 `UART_ENABLE_RX=False`。只有双向命令协议经过单独设计、MSPM0 发送端空闲电平和上电行为验证后，才允许改为 `True`。

A16 是启动模式相关引脚。MSPM0_RX 或电平转换电路不得在 MaixCAM 复位时把 A16 拉低。UART0 开机日志不是视觉协议，MSPM0 必须忽略所有不符合完整协议帧的数据。

## 2. 首版协议边界

首版只发送兼容现有主控的文本帧：

```text
AIM,valid,dx,dy,target_x,target_y,laser_x,laser_y,target_mode,laser_color\n
```

- `valid=1`：靶心和激光点都有效且未过期，`dx=target_x-laser_x`、`dy=target_y-laser_y`。
- `valid=0`：不允许 MSPM0 根据该帧更新激光闭环误差。
- 目标有效但激光无效时，保留靶心坐标并使用 `NO_LASER`；MSPM0 不得把 dx/dy 当成有效误差。
- 目标无效、`lost_hold=True` 或 `age_ms>100` 时，发送 `AIM,0,0,0,0,0,0,0,LOST,LOST`。
- UART 写失败时，视觉端不伪造成功帧；MSPM0 的接收看门狗必须在 100 ms 内停止继续跟随。

V2 首版不接收控制命令、不发送 JPEG、不启用二进制协议。二进制 CRC 协议另起版本，与 MSPM0 同步升级。

## 3. 数据字段契约

所有 `target` 和 `laser` 量测使用同一字典结构：

| 字段 | 类型/范围 | 含义 |
|---|---|---|
| `kind` | `target` 或 `laser` | 量测来源类别 |
| `valid` | bool | 当前对象可被使用 |
| `updated` | bool | 本帧是否获得真实新量测 |
| `predicted` | bool | 是否由预测产生；预测不重置真实量测时间 |
| `lost_hold` | bool | 短时保留旧值；不得作为新量测或有效 AIM 误差 |
| `x`、`y` | 像素整数 | 图像坐标 |
| `rect` | `[x,y,w,h]` 或 `None` | 候选外接框 |
| `corners` | `[(x,y), ...]` 或 `None` | 靶框按 TL、TR、BR、BL 排列的四角 |
| `confidence` | 0～100 | 模块内部置信度 |
| `timestamp_ms` | 单调毫秒 | 最后真实量测时间 |
| `age_ms` | 非负毫秒 | 从最后真实量测开始累计的年龄 |
| `reason` | 字符串 | `INIT`、`TRACK`、`RECAPTURE`、`LOST`、`STALE` 等原因 |
| `mode` | 字符串 | target 的来源或识别模式，例如 `TRACK`、`RECAPTURE` |
| `color` | 字符串 | laser 的颜色类别；405 nm 蓝紫激光的首选字段值为 `blue_violet` |

完整 `observation` 包含：`target`、`laser`、`frame_index`、`timestamp_ms`。视觉模块只产生这些结构；UART 模块只编码这些结构；电机控制不进入 V2 视觉代码。

## 4. 安全规则

1. `age_ms > 100`：目标不新鲜，视觉端发送 LOST，MSPM0 停止继续跟随。
2. `lost_hold=True`：可用于画面显示和内部重捕参考，但不得当成新量测。
3. `predicted=True`：最多持续 80 ms、最多 5 帧；随后必须失效。
4. 目标丢失、串口异常、协议无法解析或系统 UART0 被占用时，不得输出伪造的有效瞄准误差。
5. A17 未启用时，V2 不读取串口，避免把启动期或外部噪声当成命令。

## 5. 第 1 步测试场景

| 编号 | 场景 | 通过条件 |
|---|---|---|
| S1 | 无 A17，仅 A16 和 GND | V2 只映射 A16/UART0_TX，MSPM0 能收到测试帧 |
| S2 | `maix_comm_method=uart` | V2 拒绝打开 UART0，不创建 UART 对象 |
| S3 | `maix_comm_method=none` | V2 映射 A16、创建 `/dev/ttyS0` 115200 |
| S4 | 靶心和激光均新鲜 | 输出 `AIM,1,...`，dx/dy 符号正确 |
| S5 | 靶心 `age_ms=101` | 输出 LOST 帧 |
| S6 | 靶心 `lost_hold=True` | 输出 LOST 帧 |
| S7 | 靶心有效、激光无效 | 输出 `AIM,0,...,NO_LASER`，主控不更新瞄准误差 |
| S8 | 拔掉或阻断 UART | MSPM0 在 100 ms 内停止继续跟随 |
| S9 | MaixCAM 上电 | MSPM0 忽略 UART0 开机日志，直到完整 AIM 帧 |

S1～S7 可先用无硬件测试和串口工具完成；S8～S9 必须实机完成并记录结果。

## 6. 后续开发纪律

- 后续目标、几何、激光和滤波模块只能填充本契约定义的数据字段。
- 需要新字段时，先修改本文件，再同步修改单元测试和协议版本。
- 不在旧版 `TrackingCar-Vision` 中验证或修改 V2 功能。
