# V2 模块边界

V2 的模块边界固定如下：

| 层 | 可以做什么 | 不可以做什么 |
|---|---|---|
| `vision/` | 目标、激光、几何、滤波；生成统一量测字典 | 不访问 UART、不访问相机底层、不控制电机 |
| `app/` | 调度状态机、归一化量测、汇总 observation | 不实现具体图像算法、不直接操纵电机 |
| `drivers/` | 相机、UART、显示等硬件访问 | 不判断目标合法性、不定义控制策略 |
| `protocol/` | 将 observation 编码为文本或二进制帧 | 不访问相机、不产生视觉量测 |
| `tests/` | 验证数据契约、几何、协议、状态机和模块边界 | 不依赖 MaixCAM 硬件 |

所有目标和激光输出必须经过 `app.models.normalize_measurement()`，再由 `app.models.new_observation()` 组成完整观测结果。字段的权威定义见 [接口契约](interface_contract.md)。

视觉模块永远不直接调用电机、MSPM0 或 UART。电机安全由 MSPM0 实现；视觉侧只通过协议输出有效、失效、预测和年龄状态。
