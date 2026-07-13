# 2026-07-13 vision_comm C-Style 完整审查

> 本次审查合并了当天早些时候在 `mspm0/log/review_vision_comm.md` 中的初版审查意见，
> 并以代码修改后的最新版本（含 `vision_config.h`、TV 解析、`vision_packet_to_observation()`、
> 可注入 UART 配置、PRIMASK 保存恢复等）为审查基线。
> 初版中的 2.1–2.9 问题已在第二步代码中修复，此处不再作为待修复项列出。

## 检查范围

- `mspm0/vision_comm/vision_observation.h` — 协议无关目标观测合约
- `mspm0/vision_comm/vision_config.h` — 集中参数常量
- `mspm0/vision_comm/vision_packet.c/.h` — AIM / TV 文本行解析器
- `mspm0/vision_comm/vision_uart.c/.h` — UART ISR、行缓冲、观测发布
- `mspm0/vision_comm/tests/vision_packet_test.c` — 解析器白盒测试
- `mspm0/vision_comm/README.md` — 模块文档
- `docs/uart_protocol.md` — MaixCAM 串口协议参考

## 检查依据

- **C-Style 团队规范**：分层、所有权、接口显式性、struct 优先、命名一致性
- **当前阶段目标**：云台摄像头跟随目标，控制误差 = 目标位置 − 画面期望位置，不依赖激光点
- **硬件接线**：板上 UART4 接 MaixCAM（MCU UART3 / PB2 PB3）；Yaw X42S 接 UART3（MCU UART2）；Pitch X42S 接 UART5（MCU UART1）
- **MaixCAM 输出**：`AIM,...`（精瞄模式，10 字段）与 `TV,...`（目标模式，6 字段）

---

## 1. 整体架构评价

### 当前分层（已实现）

```
config:     vision_config.h         ← 编译期常量集中
contract:   vision_observation.h    ← 纯头文件，无外部依赖
protocol:   vision_packet.c/.h      ← 不触碰硬件，独立可测试
driver:     vision_uart.c/.h        ← 依赖 ti_msp_dl_config.h，注入配置
```

**评价：分层正确，模块边界清晰。**

- `vision_uart.h` 对外只暴露 `vision_observation_t`（通过 `vision_uart_take_latest_observation()`），不再泄漏 `vision_packet_t`
- `vision_packet.c` 无硬件依赖，已通过 `tiarmclang -std=c11 -Wall -Wextra -Werror` 编译并与 `tests/vision_packet_test.c` 链接
- UART 实例通过 `vision_uart_config_t` 注入，不再硬编码 `UART0`
- 临界区正确保存/恢复 PRIMASK（`copy_rx_line`），不会破坏调用方的中断屏蔽状态
- `overrun_count` 与 `parse_error_count` 已分离

### 与初版审查的对比

| 初版问题 | 当前状态 |
|---|---|
| 2.1 getter 代码重复 | ✅ 已修复：删除 `vision_uart_get_latest_packet`，改为 `vision_uart_take_latest_observation` |
| 2.2 IRQ 临界区不一致 | ✅ 已修复：`copy_rx_line` 保存并恢复 PRIMASK |
| 2.3 超限混入解析错误 | ✅ 已修复：拆分为 `overrun_count` 和 `parse_error_count` |
| 2.4 缺失 observation 转换 | ✅ 已修复：新增 `vision_packet_to_observation()`，UART 对外只发布 observation |
| 2.5 魔法数字 | ✅ 已修复：集中至 `vision_config.h` |
| 2.6 packet_t 无注释 | ✅ 已修复：添加 Doxygen 注释 |
| 2.7 uart.h 耦合 packet.h | ✅ 已修复：uart.h 改为 include `vision_observation.h` |
| 2.8 参数分散 | ✅ 已修复：新建 `vision_config.h` |
| 2.9 协议检测硬编码 | ✅ 已修复：`vision_packet_parse_line` 按首字符分派 AIM/TV |
| — 坐标零值误判 | ✅ 已修复：改为根据 `target_mode` token 判断有效性 |
| — 解析无溢出检查 | ✅ 已修复：`parse_i32` 带模值溢出检测 |
| — 缺少字段分隔符检查 | ✅ 已修复：`consume_comma_and_i32` 严格要求逗号 |

---

## 2. 发现的新问题（按影响排序）

### 🟡 应该修复（行为稳定后重构）

#### 2.1 `parse_tv_line` 丢弃 `target_mode` token 语义值

**文件**：[vision_packet.c](vision_packet.c#L192-L193)

```c
(void) target_mode;
(void) target_mode_length;
```

TV 协议的 `mode` 字段被解析（要求非空，保证格式完整性），但其**语义值被丢弃**。当前 `target_valid` 仅由 `field[0]`（即 `valid` 字段）决定：

```c
out->target_valid = (field[0] == 1);
```

如果 MaixCAM 未来发送 `TV,1,0,0,320,240,LOST`（`valid=1` 但 `mode=LOST`），解析器会标记 `target_valid=true`，导致云台跟踪一个已丢失的目标。

**建议**：为 TV 协议也定义 mode token 的白名单/黑名单，类似 AIM 的 `token_equals(target_mode, ..., "NO_TARGET")` 模式。当前 TV `mode` 的可能值应从 MaixCAM 源码确认后加入。

**风险**：低。当前 MaixCAM TV 模式下 `valid` 字段由算法直接设置，与 `mode` 一致。但如果算法逻辑变化，此处将成为潜伏 bug。

---

#### 2.2 `vision_uart.h` 对 `ti_msp_dl_config.h` 的硬依赖

**文件**：[vision_uart.h](vision_uart.h#L6)

```c
#include "ti_msp_dl_config.h"
```

`vision_uart_config_t` 使用 TI 专有类型 `UART_Regs *` 和 `IRQn_Type`，导致：
- 包含 `vision_uart.h` 的任何编译单元都必须能解析 `ti_msp_dl_config.h`
- 无法在宿主机上对 UART 模块做单元测试（与 `vision_packet.c` 的独立可测试性形成反差）

**建议**：将 `vision_uart_config_t` 中的硬件类型替换为抽象类型：

```c
typedef struct {
    void *instance;            /* UART_Regs * — cast in .c */
    int irqn;                  /* IRQn_Type — cast in .c */
    uint16_t frame_width;
    uint16_t frame_height;
} vision_uart_config_t;
```

在 `vision_uart.c` 内部做显式转换，并用 `static_assert` 或编译期检查保证类型兼容。

**权衡**：失去少量类型安全，换来可测试性和可移植性。对于已进入联调阶段的嵌入式项目，此项优先级可降低。

---

#### 2.3 `vision_packet_parse_line` 首字符分派脆弱

**文件**：[vision_packet.c](vision_packet.c#L215-L221)

```c
if (line[0] == 'A') {
    return parse_aim_line(line, out);
}
if (line[0] == 'T') {
    return parse_tv_line(line, out);
}
```

任何以 `'A'` 开头但非 AIM 的行（如未来的 `"ANGLE,..."` 协议）会被送入 `parse_aim_line`，在 `consume_prefix` 中因 `"AIM," ≠ "ANG"` 而拒绝，行为正确但路径冗余。

**建议**：在 `consume_prefix` 之后才决定协议类型，或使用两级分派（首字符 + 第二字符）：

```c
if ((line[0] == 'A') && (line[1] == 'I')) { return parse_aim_line(line, out); }
if ((line[0] == 'T') && (line[1] == 'V')) { return parse_tv_line(line, out); }
```

**风险**：当前无实际 bug，仅设计脆弱性。建议在加入第三个协议之前修掉。

---

#### 2.4 `vision_uart_process()` 写入观测结构体非原子

**文件**：[vision_uart.c](vision_uart.c#L137-L138)

```c
g_latest_observation = observation;
g_observation_ready = true;
```

`vision_observation_t` 为多字结构体（24 bytes），在 Cortex-M0+ 上的 struct copy 不保证原子性。当前安全，因为**写入者（`vision_uart_process`）和读取者（`vision_uart_take_latest_observation`）都是主循环函数**。但如果后续引入 RTOS 或将读取移到 ISR / 高优先级任务中，存在读到半写入观测的风险。

**建议**：在模块注释或 `vision_uart_take_latest_observation` 的 Doxygen 中明确标注：

```c
/**
 * @brief Consume the newest unpublished observation.
 * @details Main-loop API. Do not call it from an ISR.
 *          Must be called from the same thread/context as vision_uart_process().
 */
```

当前 Doxygen 已有 `Main-loop API. Do not call it from an ISR.`，但未说明必须与 process 在同一线程。补充即可。

---

### 🟢 可选清理

#### 2.5 `docs/uart_protocol.md` 接线描述过时

**文件**：[docs/uart_protocol.md](docs/uart_protocol.md#L92-L98)

```
MaixCAM Pro A19 TX -> MSPM0G3507 PA11 UART0_RX
MaixCAM Pro A18 RX <- MSPM0G3507 PA10 UART0_TX
```

这与当前实际接线（板上 UART4 → MCU UART3 / PB2 PB3）和 `vision_comm/README.md` 中的更新描述不一致。旧的 UART0 文档会误导新成员接线。

**建议**：同步更新 `docs/uart_protocol.md` 的 MSPM0 接线章节，或添加指向 `vision_comm/README.md` 的引用。

---

#### 2.6 `vision_packet_parse_aim_line` 可标记为废弃

**文件**：[vision_packet.h](vision_packet.h#L34-L35)

```c
/** @brief Compatibility entry point that accepts AIM lines only. */
bool vision_packet_parse_aim_line(const char *line, vision_packet_t *out);
```

该兼容接口仅用于过渡期。当前已无模块内部调用它（`vision_uart.c` 使用 `vision_packet_parse_line`）。

**建议**：如确认无外部调用者，添加 `__attribute__((deprecated))` 或在下一步清理中移除。

---

#### 2.7 测试覆盖可增强

**文件**：[tests/vision_packet_test.c](tests/vision_packet_test.c)

当前覆盖 3 个场景：TV 原点目标、AIM 无激光目标、非法行拒绝。以下边界场景尚未覆盖：

- `AIM,1,...` 且 `target_mode=LOST`、`laser_color=NO_LASER` → 应拒绝（`aim_valid` 与 token 矛盾）
- `AIM,0,...,NO_TARGET,NO_LASER` → 应接受但 `target_valid=false`
- 空字符串、仅换行符 → 应拒绝
- `TV` 行含溢出坐标值 → 应拒绝

**建议**：在进入真机联调前补充 3–5 个额外断言。

---

#### 2.8 `vision_packet_t` 有 `bool aim_valid` 和 `bool target_valid` 两个有效性标志

**文件**：[vision_packet.h](vision_packet.h#L20-L21)

```c
bool aim_valid;     /**< AIM validity: both target and laser are valid. */
bool target_valid;  /**< Target validity for this frame. */
```

两个字段名相似但语义不同：`aim_valid` 直接映射到 CSV 的 `valid` 字段（"目标和激光同时有效"），而 `target_valid` 是解析器根据 `target_mode` token 推断的。对于 TV 协议，`aim_valid` 固定为 `false`，`target_valid` 来自 `field[0]`。

从命名上不易区分「AIM 帧级有效性」和「目标级有效性」。如果后续协议不再有 AIM 特有的双有效概念，此命名将成为历史包袱。

**建议**：可考虑将 `aim_valid` 重命名为 `full_valid` 或 `dual_valid`，或加注释说明该字段在 TV 协议下恒为 false。

---

## 3. 审查检查清单

| 检查项 | 状态 | 备注 |
|---|---|---|
| **分层**：每个文件属于正确的层 | ✅ | config / contract / protocol / driver 四层清晰 |
| **所有权**：每个可写全局变量有唯一所有者 | ✅ | `g_rx_*` → ISR；`g_latest_observation` → `vision_uart_process()`；`g_stats` → `vision_uart_process()` |
| **接口**：函数签名能看出依赖和副作用 | ✅ | `vision_uart_process(now_ms)` 表明需要时基；`take_latest_observation` 表明一次性消费 |
| **Struct**：关联变量已分组为结构体 | ✅ | `vision_uart_config_t`、`vision_uart_stats_t`、`vision_packet_t`、`vision_observation_t` 各司其职 |
| **Header**：`.h` 只暴露预期的公共契约 | ✅ | `vision_uart.h` 不再暴露 `vision_packet_t`；`vision_packet.h` 暴露转换函数但它是 protocol 层的合法公共接口 |
| **Static**：`static` 隐藏实现细节 | ✅ | `push_rx_byte`、`copy_rx_line`、`parse_i32`、`consume_prefix` 等合理标记为 `static` |
| **参数**：可调常量集中并用角色命名 | ✅ | `vision_config.h` 集中了所有协议和缓冲参数 |
| **重复**：重复逻辑是否应该合并 | ✅ | 无显著重复；AIM 和 TV 解析器因语义差异不适合强行合并 |
| **Main**：`main.c` 只做初始化和调度 | ✅ | README 示例正确 |
| **命名**：文件名、函数名、类型名匹配行为 | ✅ | `vision_uart_take_latest_observation`、`vision_packet_parse_line` 等命名准确 |
| **风险**：改动一个模块时，哪些会意外受影响 | ⚠️ | 改 `vision_packet_t` 结构体会影响 `vision_uart.c` 和所有调用 `vision_packet_parse_line` 的代码（这是正常的层间依赖） |

---

## 4. 建议的目标结构

```
vision_comm/
├── vision_config.h              ← 集中参数（已有）
├── vision_observation.h         ← 协议无关合约（已有，保持不变）
├── vision_packet.h              ← 协议解析器接口（已有，可考虑废弃兼容接口）
├── vision_packet.c              ← 协议解析器实现（已有，补充 TV mode 语义检查）
├── vision_uart.h                ← UART 模块接口（已有，可考虑降低 ti_msp_dl_config 依赖）
├── vision_uart.c                ← UART 模块实现（已有，补充线程安全文档）
├── tests/
│   └── vision_packet_test.c     ← 解析器测试（已有，可扩展用例）
└── README.md                    ← 模块文档（已有，已更新接线）
```

---

## 5. 低风险迁移顺序

每一步保持项目可编译：

1. **（本次即可）** 在 `vision_uart_take_latest_observation` Doxygen 中补充同线程要求
2. **（本次即可）** 同步 `docs/uart_protocol.md` 的接线描述
3. **（本次即可）** 补充 `tests/vision_packet_test.c` 的边界测试用例
4. **（稳定后）** 为 `parse_tv_line` 添加 `mode` token 语义检查（需先确认 MaixCAM TV mode 的可能值）
5. **（稳定后）** 将 `vision_uart.h` 中的硬件类型改为抽象类型
6. **（新增协议时）** 加固 `vision_packet_parse_line` 的两字符分派
7. **（确认无调用者后）** 移除 `vision_packet_parse_aim_line` 兼容接口

---

## 6. 已修复问题汇总（来自初版审查和首次接口审查）

以下问题已在当前代码中修复，记录于此供参考：

| 来源 | 问题 | 修复方式 |
|---|---|---|
| 初版 2.1 | getter 代码重复 | 删除 `vision_uart_get_latest_packet`，统一为 `vision_uart_take_latest_observation` |
| 初版 2.2 | IRQ 临界区不一致 | `copy_rx_line` 保存/恢复 PRIMASK；主循环状态不加无意义的关中断 |
| 初版 2.3 | 超限混入解析错误 | 拆分 `overrun_count` / `parse_error_count` |
| 初版 2.4 | 缺失 observation 转换 | 新增 `vision_packet_to_observation()` |
| 初版 2.5 | 魔法数字 | 集中至 `vision_config.h` |
| 初版 2.6 | packet_t 无注释 | 添加 Doxygen 注释 |
| 初版 2.7 | uart.h 耦合 packet.h | 改为 include `vision_observation.h` |
| 初版 2.8 | 参数分散 | 新建 `vision_config.h` |
| 首次审查 1 | `aim_valid` 语义不适合跟踪 | 解析器根据 `target_mode` token 产生 `target_valid`；跟踪层从 observation 读取 |
| 首次审查 2 | UART 实例硬编码 | 改为 `vision_uart_config_t` 注入 |
| 首次审查 3 | 无数据新鲜度/超时语义 | observation 携带 `sequence` 和 `received_at_ms` |
| 首次审查 4 | 单行缓存在阻塞时丢帧 | 记录 `overrun_count`；环形缓冲列为下一步 |
| 首次审查 5 | 解析无溢出检查/畸形帧被接受 | `parse_i32` 带模值溢出检测；严格要求逗号分隔符；越界拒绝整帧 |
| 首次审查 6 | 坐标零值误判为无目标 | 改为根据 `target_mode` / `valid` 字段判断有效性 |
| 首次审查 7 | `__enable_irq()` 破坏调用方临界区 | 保存并恢复 PRIMASK |
| 首次审查 8 | README 接线与实际不一致 | 已更新为板上 UART4 → MCU UART3 / PB2 PB3 |

---

## 7. 未采纳建议

- **不公开内部 `static` 辅助函数**（见 `2026-07-13_vision-comm-review-disposition.md` 发现 8）。保留 `static`，通过 `tests/vision_packet_test.c` 的公开解析入口做黑盒测试。
- **环形缓冲不在本次实现**（同上，发现 9）。先记录独立的 `overrun_count`，下一步在保持协议接口不变的条件下改为环形缓冲。
- **不替换文本协议为二进制 CRC 协议**（见首次审查）。当前先兼容 MaixCAM 已有输出；闭环跑通后再升级。
- **不修改 MaixCAM 目标检测算法**。本次审查范围为 MSPM0 通信接口。

---

## 8. 验证情况

- ✅ `vision_packet.c` + `tests/vision_packet_test.c` 已通过 `tiarmclang -std=c11 -Wall -Wextra -Werror` 编译，无诊断。
- ✅ `git diff --check` 通过，无空白错误。
- ⚠️ 测试目标为 ARM 交叉编译产物，当前环境未连接开发板，`assert` 用例尚未实际执行。
- ⚠️ 未进行真机 UART、MaixCAM 或步进电机测试；需在接线和 SysConfig 完成后验证。

---

## 9. 总体评价

**此模块已达到可联调的质量水平。** 分层架构清晰，模块边界符合 C-Style 规范要求。初版审查的全部 9 项问题 + 首次接口审查的全部 8 项问题均已在代码中修复。当前剩余的 4 个「应该修复」项均为防御性改进和文档同步，不影响联调功能。3 个「可选清理」项为锦上添花。

建议在进入云台跟踪控制联调前，优先完成 2.1（TV mode 语义检查）和 2.4（线程安全文档），以降低联调中出现意外行为的概率。

## 依据与工具

- Skill: `C:\Users\Aupassen\.claude\skills\c-style\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\vision_comm\vision_observation.h`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\vision_comm\vision_config.h`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\vision_comm\vision_packet.c`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\vision_comm\vision_packet.h`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\vision_comm\vision_uart.c`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\vision_comm\vision_uart.h`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\vision_comm\tests\vision_packet_test.c`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\vision_comm\README.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\docs\uart_protocol.md`
- Reference: `C:\Users\Aupassen\Desktop\视觉\.log\review\2026-07-13_mspm0-vision-uart-interface-review.md`
- Reference: `C:\Users\Aupassen\Desktop\视觉\.log\review\2026-07-13_vision-comm-review-disposition.md`
