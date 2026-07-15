# 双轴视觉云台集成复盘与比赛拼装手册

日期：2026-07-15  
适用工程：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm`

## 1. 项目当前达到的状态

工程已经完成从 MaixCAM 文本观测到两路 X42S 步进电机运动的完整链路：主控能接收目标坐标，按 320×240 画面中心计算 `dx/dy`，分别驱动 Yaw 和 Pitch；两轴具备开机小角度自检、非阻塞收发、视觉 PD、短时丢失延续和无目标扫描。

已用电脑串口仿真代替摄像机做过 Yaw-only、Pitch-only、Dual 和连续轨迹测试。扩栈并修正轴绑定后，100 Hz、250 Hz 以及 100 Hz 连续三轮均未出现命令拒绝、TX 超时、协议错误或保护错误，用户也在 Dual 阶段观察到两轴同时转动。摄像机重新接入后，两轴方向和跟踪平滑性经过实机调试，25 ms PD 已达到可用状态。

当前仍属于比赛前调试版，不是最终封板版。人工机械零位、绝对软限位、摄像机通信看门狗和比赛日志降载尚未完成。

## 2. 现行数据流和控制逻辑

```text
MaixCAM 检测算法
    ↓ AIM/TV 文本，换行结帧
UART0 PA1 RX 中断
    ↓ 256 B 环形缓冲
vision_uart_process()
    ↓ vision_packet_parse_line()
vision_observation_t（统一观测接口）
    ↓ dx=x-160，dy=y-120
target_recovery_control
    ├─ TRACKING：使用当前 dx/dy
    ├─ PREDICTING：300 ms 内复用上一有效 dx/dy
    └─ SCANNING：Yaw ±90°、Pitch ±30°往返
    ↓
Yaw/Pitch 各自的 pitch_tracker_control（PD 外环）
    ↓ 相对实时位置 pulse 命令
pitch_motor_control
    ↓
stepper_motor → zdt_x42s
    ↓ 独立 TX 状态、独立 RX 应答解析
UART1 PB6/PB7（Yaw） / UART2 PA23/PA24（Pitch）
```

X42S 电机利用自身编码器完成内部位置闭环。MSP 上的 PD 不是重复实现电机位置 PID，而是视觉外环：把像素误差换成下一次相对位置修正。这个分层必须保留，否则会把“图像回中”和“电机到位”混在一个控制器里。

主循环采用协作式调度，不是真正的多线程：每个模块都必须快速返回，通过 `now_ms` 决定本轮是否工作。主循环持续轮询视觉、两轴返回、两轴 TX 和心跳；任何一个模块都不能等 UART FIFO、等应答、等到位或 `delay`。

## 3. 当前硬件与参数基线

| 功能 | 引脚/实例 | 参数 |
| --- | --- | --- |
| MaixCAM | PA0/UART0_TX，PA1/UART0_RX | 115200 8N1，换行结帧 |
| Yaw | PB6/UART1_TX，PB7/UART1_RX | 地址 1，X42S Emm 位置模式 |
| Pitch | PA23/UART2_TX，PA24/UART2_RX | 地址 1，X42S Emm 位置模式 |
| 心跳 | PB22 | 每 500 ms 翻转 |
| SysTick | Cortex-M SysTick | 1 ms 单调时基 |
| 启动栈 | Keil startup | 4 KiB |

控制基线：25 ms；死区 ±4 px；Yaw `Kp=0.40`、`Kd=0`、正误差方向为非 CW；Pitch `Kp=0.40`、`Kd=0.005`、正误差方向为 CW；两轴单次最大 400 pulse。实际方向必须在装机后用误差是否收敛复核，不能把布尔值当成跨机械结构通用常量。

开机自检按 3200 pulse/rev 计算约 267 pulse，即约 30°；先正向再反向回到启动位置。若电机与云台之间不是 1:1 传动，应按实际减速比重算，不能沿用 267 pulse。

## 4. 遇到的问题、证据和根因

### 4.1 烧录成功但程序像没运行

现象包括：直接下载后 LED 不闪，进入 Debug 后运行却正常；单步光标停在启动汇编的 `BX R0`；下载后要按 RESET 或真正断电重上电才开始。

这里有两个问题被混在一起：一是 Keil 单步看到 `BX R0` 只是从 Reset Handler 跳到 `main`，不是卡死；二是下载器或调试器仍可能给 MCU 供电，仅关闭电机电源不等于 MCU 发生上电复位。最终用 PB22 心跳作为固件活性证据，并把“下载后按板上 RESET/真正 MCU 断电”写入操作流程。Debug 模式执行 `G` 能直接启动，是因为调试器明确释放内核运行。

以后遇到“外设全无反应”，先看心跳，不要先改 UART 或电机代码。

### 4.2 `g_monotonic_ms` 单步时一直是 0

SysTick 中断只有在内核运行时才持续触发。停在断点并反复 F10，毫秒计数可能不变；让程序自由运行后计数正常增长。这个现象不能用来证明 SysTick 配置错误。观察时间状态时应 Run 一段时间再 Halt，看 Watch 值是否增长。

### 4.3 串口有乱码、只有 0x00/0xFF/0xC0

电机协议是二进制帧，在 ASCII 模式下必然显示乱码。波特率、8N1、HEX/ASCII显示方式和共地必须分开核对。只连接 MCU TX 与 TTL RX 可以观察发送，但无法验证电机返回；闭环诊断必须再连接电机 TX 到 MCU RX。

开关电源瞬间出现孤立字节不是有效协议证据，可能是线路边沿、接收端浮空或供电时序造成。有效证据应是重复的完整 HEX 帧，并满足地址、命令和帧尾校验。

### 4.4 PA23 始终没有 UART 波形

这是整个排障中最有代表性的硬件问题。软件初始化、复用和波特率都被反复检查过，两块板的表象又相似，容易把问题归因到“PA23 是特殊引脚”、VREF+、上下拉或 SDK。

最有判别力的实验是把 PA23 临时配置成普通 GPIO，每秒高低翻转，同时让 PB22 心跳证明新固件确实运行。结果 PB22 正常，PA23 始终约 3.35 V；万用表进一步测出 PA23 与 3V3 短路。最终发现开发板背面两个测试点连锡。清除连锡后 PA23 GPIO 能翻转，UART 也恢复。

结论：复用配置正确并不代表引脚外部网络正常。遇到无波形时，应按“固件活性 → GPIO 强制翻转 → 静态电阻/短路 → UART 复用”的顺序查。不要用内部下拉对抗外部硬短路，也不要在缺少电气证据时归咎芯片特殊功能。

### 4.5 看似没有 UART 初始化

早期怀疑 `SYSCFG_DL_init()` 没有初始化电机 UART，但审查生成代码后确认 UART1/UART2、MFCLK、115200、FIFO、引脚复用和使能调用都存在。真正的问题一度是“诊断函数用错 UART 实例”或“封装函数根本没有调用点”。

经验：检查 UART 要沿完整调用链，而不是只搜索 `init`：

1. SysConfig 是否生成实例和引脚。
2. `SYSCFG_DL_init()` 是否调用该实例初始化。
3. `main` 中配置对象绑定了哪个实例。
4. 周期函数是否真的被调用。
5. 发送函数是否把字节写到同一实例。
6. Keil 工程是否编译的是这份源文件，而不是另一个目录的副本。

### 4.6 工程目录混乱导致“改了但没效果”

开发过程中同时存在 `视觉\mspm0`、`代码整理` 和 `empty_routine_vision_comm`。曾出现修改一个目录、Keil 烧录另一个目录工程的风险，也出现远程仓库看不到 `main.c` 的疑问。

最终约定权威烧录工程为 `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`。比赛时必须在操作记录里同时写绝对工程路径、`.uvprojx` 路径和当前 Git 状态。不能只说“当前代码”或“视觉文件夹”。

### 4.7 自检能双轴转，视觉闭环只能动一轴

这个现象曾引出频率太高、电源不足、两个 UART 互相影响、命令覆盖、参数限幅和电机性能等多个猜想。单轴 A/B、交换接线、交换发送顺序和电脑仿真把问题逐层缩小。

最终确认的两个主要软件根因：

- 物理轴与软件实例绑定曾经相反，造成“只有 dx/dy 时哪根轴动”的观察被错误命名。
- Keil 启动文件只分配 256 B 栈。视觉 `TV`、电机 `MS` 的 `snprintf`、函数调用和中断嵌套使栈越界，表现为运行一段时间后 LED、日志和电机全部停止。打开更多诊断时更快停止，关闭诊断时晚一些停止，正好符合栈余量变化。

扩栈到 4 KiB并修正绑定后，100 Hz、250 Hz 和连续三轮 100 Hz 测试均完成。两轴最终计数对称，`J/T/P/E=0`，用户看到 Dual 阶段两轴同时旋转。因此当前证据不支持“两个电机不能并行”“250 Hz 必然饿死调度”或“电源瞬时电流不足”是本轮根因。

### 4.8 RAM 会不会越用越满

裸机 C 程序的静态/BSS 区在启动时初始化，栈是重复使用的一段内存，不会因为主循环运行时间变长而正常地单向填满。之前“运行一会儿停”的原因不是需要周期清 RAM，而是栈边界从一开始就不够，深调用覆盖了相邻内存。

永久运行依赖边界正确，而不是定时清内存：避免大局部数组和递归，控制 `snprintf`，检查 map/call graph，设置合理栈空间，必要时填充栈哨兵测峰值；对环形缓冲和队列做满时策略与计数。清 RAM 会破坏状态，不能修复越界。

### 4.9 摄像机接电时自检异常，拔信号线正常

MaixCAM 上电会连续输出文本。如果视觉 UART 在电机自检前启动，高频 RX、解析和调试回传会与自检并发，早期阻塞路径和小栈更容易暴露问题。出现过大量 `VU,...` 异常数值，本质上是启动时序、诊断格式和缓冲状态混在一起，不能把这些数字直接当成有效目标。

现行做法是先运行两个独立非阻塞自检状态机；两者均 COMPLETE 后清 UART0 RX FIFO、初始化视觉接收并发出 `VISION_READY`。这样摄像机可以先上电，但主控在机械自检结束前不消费其启动数据。

长期还应加入摄像机帧新鲜度看门狗：超过规定时间没有完整帧，应进入受控搜索或安全停止，而不是无限沿用旧坐标。

### 4.10 阻塞发送让系统“跑一会儿卡住”

早期发送代码含等待 TX FIFO 的循环，视觉日志也曾逐行阻塞发送。单轴、自检或低频时可能看不出问题；两轴加视觉后，一个 UART 或接收端状态异常就能拖住整个主循环，心跳也随之停止。

最终采用每通道独立的 `tx_frame/tx_index/tx_length/tx_started_ms`。请求函数只在空闲时复制帧；`service_tx()` 每轮只把 FIFO 当前能接收的字节写入；超过 20 ms 清除该帧并累计 timeout。一个轴忙只会拒绝本轴新帧，不等待，也不阻塞另一轴。

注意：当前 `service_tx()` 内仍有一个“在 FIFO 未满时填满 FIFO”的短循环。它的上界由硬件 FIFO 和单帧长度限制，不是等待型死循环；如果未来换成长帧 DMA 或不同 UART 驱动，应重新审查执行上界。

### 4.11 命令频率、覆盖和顿挫

200 ms 控制周期用于早期防止命令重叠，但视觉跟踪明显一顿一顿。降低到 50 ms 后改善，最终用 25 ms 配合较小 PD 输出得到平滑效果。

频率不是越高越好。115200 8N1 下，一个约 45 B 的 AIM 帧在线路上约占 3.9 ms；250 Hz 已接近连续占线。电脑 250 Hz 压力测试证明当前实现不死锁，但比赛摄像机无需强行跑到链路极限。正确结构是持续消费 RX，只保存最新观测，控制器按固定周期运行；而不是每收到一帧就必须下发一次电机命令。

### 4.12 目标被转到右下角而不是中心

控制最初按 512×320 计算中心 `(256,160)`，而实际拆分坐标为 320×240。目标到达真实中心 `(160,120)` 时，软件仍认为存在巨大负误差，因此持续把云台推向角落。修正为 320×240 后，参考中心才是 `(160,120)`。

方向问题与中心问题必须分开。方向正确的判据是施加小动作后对应 `|dx|` 或 `|dy|` 减小；中心正确的判据是目标在物理画面中心时误差接近零。两者同时错误会形成很强的误导。

### 4.13 比例过大、越过目标并丢失

初期 pulse/px 与单次限幅过大，目标一旦偏离就产生大角度相对位置命令，电机继续完成旧动作时摄像机已经越过目标。随后逐步降低比例、限制自检和控制幅度，再用自动扰动—回正试验调 PD。

最终参数不是理论最优，而是当前机构、速度、相机帧率和目标条件下的实机基线。更换镜头视场、云台惯量、电机细分或减速比后必须重调。`Kd` 依赖真实 `dt`，不能从 25 ms 直接搬到其他控制周期而不重新观察。

### 4.14 目标丢失后的处理

直接停止会丢掉快速目标的运动趋势；无限沿用最后误差则会失控。因此当前分三态：

- `TRACKING`：有效目标，正常 PD。
- `PREDICTING`：首次失效后 300 ms，沿用最后一帧 `dx/dy`。
- `SCANNING`：超过 300 ms，停止跟踪器并按范围往返搜索。

扫描以“进入扫描时的位置”为零，内部只累计被电机接口接受的 pulse；命令被拒绝时不更新虚拟位置，避免软件位置与实际发送脱节。重新找到目标时停止扫描并重置跟踪器历史，防止旧导数造成冲击。

风险是没有绝对机械零位和限位开关：多次丢失后，每次扫描中心可能不同；手动搬动机构也无法被主控感知。人工基准和绝对软限位是下一阶段优先项。

## 5. 模块化边界

### 5.1 视觉协议层

`vision_comm` 的输出只能是 `vision_observation_t`。MaixCAM 算法可以更换颜色块、模板、神经网络或队友的任意实现，只要串口适配器最终填充：

- `target_valid`
- `target_x/target_y`
- `frame_width/frame_height`
- `sequence`
- `received_at_ms`

控制层不得解析 `blob-fallback`、`NO_LASER` 等算法私有字符串。新增协议时只改 `vision_packet` 或增加适配器。

### 5.2 电机协议层

`zdt_x42s` 只负责字节帧、非阻塞发送和四字节返回解析；`stepper_motor` 提供与具体业务无关的 enable/move/stop；`pitch_motor_control` 负责带符号 pulse、诊断计数和自检状态机。虽然文件名仍含 `pitch`，它实际被 Yaw/Pitch 两轴复用，后续可重命名为 `gimbal_axis_motor`，但应作为一次独立重构，不能在比赛前临时大改。

每个电机对象必须包含自己的 UART、发送缓冲、索引、超时、返回缓冲和计数。禁止把这些状态放到函数内共享 `static` 变量，否则双轴会互相覆盖。

### 5.3 控制层

`pitch_tracker_control` 实际是通用单轴视觉 PD。每轴独立保存上次误差、时间、死区、增益、限幅、方向和下发周期。双轴控制只是对两个实例分别调用，不应写一个等待 Yaw 完成再处理 Pitch 的串行函数。

`target_recovery_control` 不直接访问 UART 或电机，只输出“是否用 tracker、扫描多少 pulse、是否停机/重置”。这种纯状态机边界便于用 PC 单元测试，也便于比赛时替换搜索策略。

### 5.4 应用与配置层建议

下一次比赛拼装时建议把 `main.c` 中的业务静态变量收进一个 `gimbal_app_t`，形成以下接口：

```c
bool gimbal_app_init(gimbal_app_t *app, const gimbal_app_config_t *config);
void gimbal_app_on_observation(gimbal_app_t *app,
                               const vision_observation_t *observation);
void gimbal_app_update(gimbal_app_t *app, uint32_t now_ms);
void gimbal_app_get_status(const gimbal_app_t *app,
                           gimbal_app_status_t *status);
bool gimbal_app_set_manual_zero(gimbal_app_t *app);
```

`gimbal_app_config_t` 至少包含两轴 UART/地址、图像尺寸、方向、Kp/Kd、死区、限幅、周期、搜索范围、传动比和安全行程。`main` 只做板级初始化、取视觉观测和调用 `gimbal_app_update()`。这样巡线、按键、激光和云台都能在同一非阻塞调度框架内拼装。

## 6. 标准排障顺序

### 6.1 不接电机和摄像机

1. 全量编译，确认使用正确 `.uvprojx`。
2. 下载后按 RESET，观察 PB22 心跳。
3. Watch `g_monotonic_ms`，Run 后 Halt，确认递增。
4. 必要时把目标 TX 临时改为 GPIO，每秒翻转并测排针。

### 6.2 只接 USB-TTL

1. 共地；TTL RX 接被测 MCU TX。
2. 115200、8N1；电机帧切 HEX，视觉帧切 ASCII。
3. 发送固定、可预测帧，不先运行完整控制逻辑。
4. 若要测主控接收，再接 TTL TX 到 MCU RX。
5. 串口只能被一个程序占用；Python 采集时关闭串口助手。

### 6.3 只接电机

1. 先 enable，读接受应答。
2. 单轴小角度正反自检，确认物理轴名和方向。
3. 双轴同时自检，确认两轴独立 TX/RX。
4. 出现保护帧立即停止，不继续压力测试。

### 6.4 接电脑视觉仿真

1. 等 `VISION_READY`，最长 15 s。
2. 依次发 CENTER、Yaw-only、Pitch-only、Dual 正负误差。
3. 观察 `TV` 的 `dx/dy`、`CS`、`RM`。
4. 打开低频 `MS`，比较两轴 `Q/J/F/T/A/R/P/E`。
5. 先实际摄像机频率，再做 100 Hz/250 Hz 压力测试。

计数含义：`Q` 命令排队，`J` 忙拒绝，`F` TX 完成，`T` TX 超时，`A` 接受应答，`R` 到位应答，`P` 保护错误，`E` 协议错误。正常基线是 `ΔQ>0`、`ΔF≈ΔQ`、`T/P/E=0`；当前使能帧会让 `F/A` 比运动 `Q` 固定多 1。

### 6.5 最后接 MaixCAM

1. MaixCAM TX 接 PA1，GND 共地；需要主控回传时 PA0 接 MaixCAM RX。
2. 先用电脑直接监听摄像机 3—5 min，确认每行换行、坐标范围和发送频率。
3. 再接主控，等待自检结束与 `VISION_READY`。
4. 把目标放中心，确认 `(x,y)≈(160,120)`。
5. 分别偏移 X、Y，小幅验证误差减小方向。
6. 最后才允许大范围移动和搜索扫描。

## 7. 编译、下载和自动测试经验

Keil 工程：

`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\keil\empty_LP_MSPM0G3507_nortos_keil.uvprojx`

下载成功的日志应包含 Erase、Programming 和 Verify OK。普通 Flash 后若不运行，按板上 RESET 或让 MCU 真正断电；只切断电机电池不一定会让被 XDS110 供电的 MCU 复位。Debug 脚本执行 `G` 能下载后直接运行，适合自动参数试验。

电脑仿真优于 MCU 自喂数据，因为它同时覆盖真实 UART0 接收中断、环形缓冲、文本解析和控制调度。脚本必须保存原始串口日志和 JSON 统计；采集器要等待新一轮状态开始，不能误把上一次残留的 COMPLETE 当成当前测试结束。

自动测试只负责提供可重复输入和读取计数，机械安全仍需人在场。发现限位接近、线缆缠绕、保护帧或异常持续转动，应立即断电。

## 8. 下一步应完成的事项

### P0：人工基准与安全范围

增加一个明确的人工置零动作，例如长按开发板 B21：先非阻塞停止两轴，确认发送完成后向两台电机发送“清除当前位置/置零”协议，再把扫描和绝对软限位绑定到该零位。已查到的 EMM V5 清零帧示例为地址 1 时 `01 0A 6D 6B`，返回示例 `01 0A 02 6B`；实现前必须再以所用电机固件手册和实机返回确认，不能只凭镜像资料直接投入比赛。

还应配置每轴允许的绝对最小/最大角度。没有回零传感器时，人工零位在掉电、失步或手动搬动后不再可信，至少要提供重新置零流程和物理急停。

### P1：比赛版日志与看门狗

- 关闭逐帧 `TV` 和自动 PD 文本；保留低频状态摘要。
- 将电机 `P/E/T` 和视觉 overrun/parse error 设为可查询计数。
- 加视觉帧超时：超时进入安全搜索或停止。
- 加主循环活性/独立看门狗，不能只依赖 PB22。
- 用栈填充或 Keil 调用图测一次峰值，给后续巡线和显示模块留余量。

### P1：配置集中化

把引脚、实例、轴方向、传动比、PD、搜索范围和自检开关集中到一个板级配置文件。当前参数散落在 `main.c`、`pitch_motor_control.c` 和 SysConfig，移植时容易只改其中一处。

### P2：命名和测试

- 把通用的 `pitch_motor_control`、`pitch_tracker_control` 重命名为轴通用模块。
- 给 `target_recovery_control` 写主机单元测试，覆盖边界、命令拒绝和重新捕获。
- 为 AIM/TV 解析增加异常行、超长行、缺换行和坐标越界测试。
- 在真实整车任务中验证巡线、显示、按键与云台同时运行时的最坏主循环时间。

## 9. 比赛前一分钟检查表

- Keil 打开的工程绝对路径正确。
- Git 工作区状态已记录，当前固件版本可回退。
- PB22 心跳正常，下载后执行过 RESET。
- MaixCAM 115200 8N1，TX→PA1，双方共地。
- Yaw PB6/PB7，Pitch PA23/PA24，没有互换。
- PA23 背面测试点无连锡，线束无 3V3 短路。
- 两轴上电自检范围安全，线缆不会缠绕。
- 图像确为 320×240，中心是 `(160,120)`。
- X 偏移只让 Yaw 误差减小，Y 偏移只让 Pitch 误差减小。
- 高频调试输出关闭，`T/P/E` 为零。
- 人工零位和机械安全范围已重新确认。
- 现场有人能立即切断电机电源。

## 10. 方法上的反思

有效的排障不是“多改几个地方再试”，而是每次构造一个能排除一层假设的实验。PB22 + PA23 GPIO 翻转一次就区分了固件、复用和硬件短路；交换物理接线区分了轴对象与电机本体；Yaw-only/Pitch-only/Dual 区分了单轴控制与并发；关闭/打开 `MS` 后停机时间变化引出了栈；电脑固定轨迹让摄像机算法不再成为变量。

低效之处主要有三点：早期在多个工程目录间修改；连续更改 UART、引脚、发送实现和方向，导致因果难追；过度依赖肉眼“哪根轴动”，没有先把轴绑定和计数器固定。以后应先建立权威工程、接线表和诊断计数，再执行单变量实验。所有临时测试开关都要在日志中标明“当前有效”或“历史试验”，避免下一次从旧日志恢复错误参数。

这套工程真正可复用的部分不是某一组 Kp/Kd，而是四个边界：统一视觉观测、每轴独立状态、非阻塞主循环、可量化诊断。只要这四点保持，视觉算法、电机通道和上层整车任务都可以模块化替换。

## 依据与工具

- Skill：`C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Skill reference：`C:\Users\Aupassen\.codex\skills\project-logbook\references\project-reflection-log.md`
- Skill：`C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill：`C:\Users\Aupassen\.codex\skills\stop-slop\SKILL.md`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\change\README.md`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\review\2026-07-15_dual-axis-root-cause-review.md`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\review\2026-07-14_gimbal-uart-send-path-audit.md`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\.log\review\2026-07-14_vision-pitch-integration-review.md`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\main.c`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\empty.syscfg`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\keil\startup_mspm0g350x_uvision.s`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\vision_comm\vision_uart.c`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_motor_control.c`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\pitch_tracker_control.c`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\target_recovery_control.c`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\maixcam_uart_simulator.py`
- Source：`C:\Users\Aupassen\Desktop\empty_routine_vision_comm\tools\pd_tuning_logs\`
- Source：用户在 2026-07-13 至 2026-07-15 的实机接线、示波/万用表、串口和机械运动观察
- Tool：PowerShell `Get-ChildItem`、`Get-Content`、`Select-String`、`git status`，cwd `C:\Users\Aupassen\Desktop\empty_routine_vision_comm`
