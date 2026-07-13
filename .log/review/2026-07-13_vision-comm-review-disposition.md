# 2026-07-13 vision_comm 审查意见处置

## 检查范围

- `mspm0/log/review_vision_comm.md` 中 2.1 至 2.10 的审查意见。
- `mspm0/vision_comm/` 的协议、UART 和公共观测接口。

## 检查依据

- 当前 MaixCAM 输出的 `TV` 与 `AIM` 文本格式。
- 云台控制层只依赖 `vision_observation_t` 的既定模块边界。
- C 模块的 ISR/主循环单一写入者规则。

## 发现

1. 旧 UART 接口重复暴露完整数据与包数据，且把协议包类型泄漏给调用者。
   - 影响: 追踪层会被 AIM 文本格式绑定，后续切换 TV 或视觉算法时需要改控制代码。
   - 处理: 采纳。删除重复 getter，改为一次性消费 `vision_observation_t`。
2. `g_latest` 只由主循环写入，getter 中的全局关中断不能保护任何实际竞争。
   - 影响: 容易误导并破坏调用者已有的中断屏蔽状态。
   - 处理: 采纳。主循环状态不再在 getter 中关中断；仅复制 ISR 生产的行缓存时保存并恢复 PRIMASK 状态。
3. 行缓冲溢出和协议解析失败原先共用错误计数。
   - 影响: 无法区分主循环调度不足和视觉协议不兼容。
   - 处理: 采纳。拆分为 `overrun_count` 与 `parse_error_count`。
4. `vision_packet_t` 未转为 `vision_observation_t`，且尚不支持 TV。
   - 影响: 不能满足视觉代码改变而云台控制层保持不变的目标。
   - 处理: 采纳。增加 AIM/TV 解析与 `vision_packet_to_observation()`；UART 对外只发布观测结构。
5. 协议字段数量和前缀长度是未命名常量。
   - 影响: 修改协议时容易遗漏关联位置。
   - 处理: 采纳。集中至 `vision_config.h`。
6. 协议字段缺少单位和有效性说明。
   - 影响: `AIM.valid` 与目标有效性容易混淆。
   - 处理: 采纳。为 `vision_packet_t` 添加 Doxygen 注释。
7. 解析器接受数值时可能发生有符号溢出，且以坐标是否为零判断是否有目标。
   - 影响: 畸形输入可能产生未定义行为；画面左上角目标会被误判丢失。
   - 处理: 采纳。改为带范围检查的数值解析；AIM 根据 `target_mode`、TV 根据 `valid` 判断目标。
8. 建议为内部 static 辅助函数建立公共白盒测试接口。
   - 影响: 会将实现细节扩散到公共头文件。
   - 处理: 暂不采纳。保留 static，实现通过 `tests/vision_packet_test.c` 的公开解析行为做黑盒测试；只有定位到辅助函数复用需求时再抽取独立模块。
9. 建议立即将单行缓存改为环形缓冲。
   - 影响: 当前高频数据下仍可能丢弃整行。
   - 处理: 暂不在本步实现。先记录独立 `overrun_count`，下一步在保持协议接口不变的条件下改为环形缓冲并进行串口压力验证。

## 已采纳

- 审查项 2.1 至 2.9（除“单行缓存立即改环形缓冲”这一额外架构改造）已在第二步代码中实现。
- UART 绑定改为 `vision_uart_config_t` 注入，不再硬编码 UART0；这样可在应用层绑定板载 UART4 对应的 MCU UART3。

## 未采纳

- 不公开内部解析辅助函数，原因见发现 8。
- 环形缓冲不与本次协议边界改造混合，原因见发现 9。

## 验证情况

- 已用 `C:\ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmclang.exe` 以
  `-std=c11 -Wall -Wextra -Werror` 编译 `vision_packet.c` 与
  `tests/vision_packet_test.c`，无诊断通过。
- 已执行 `git diff --check`，无空白错误；未发现 `vision_comm` 根目录中对旧 UART 公共 API 的引用。
- 测试目标为 ARM 交叉编译产物，当前电脑未连接可运行该二进制的开发板，因此 `assert` 用例尚未实际执行。
- 未进行真机 UART、MaixCAM 或步进电机测试；需要在接线和 SysConfig 完成后验证。

## 依据与工具

- Skill: `C:\Users\Aupassen\.codex\skills\c-style\SKILL.md`
- Skill: `C:\Users\Aupassen\.codex\skills\project-logbook\SKILL.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\mspm0\log\review_vision_comm.md`
- Source: `C:\Users\Aupassen\Desktop\视觉\maixcam\drivers\uart_output.py`
- Tool: `functions.apply_patch`, cwd `C:\Users\Aupassen\Desktop\视觉`
- Tool: `functions.shell_command`, command `tiarmclang.exe -std=c11 -Wall -Wextra -Werror ... -c`, cwd `C:\Users\Aupassen\Desktop\视觉`
