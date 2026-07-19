# 2026-07-15 deep-module-review 处理结论

## 检查范围

- `.log/review/2026-07-15_deep-module-review.md` 的 4 项新发现。
- 当前 `main.py`、`app/pipeline.py`、`protocol/ascii_aim.py`、`protocol/safety.py`、`config.py` 和协议测试。

## 检查依据

- 用户授权按当前判断修改。
- MSPM0 100 ms 看门狗安全约束。
- 不削弱两米级远距离首捕和逐帧激光检测的要求。
- 当前实机 P95 与已有 67 项桌面测试。

## 发现

1. `MSPM0_STALE_MS` 未进入生产编码链路属实。
   - 影响: 当前默认值同为 100 ms，所以现时行为没有偏差；未来只改配置时会出现配置与编码器不同步。
   - 证据: 修改前 `main.py` 直接将 `ascii_encode` 交给 Pipeline，编码器默认参数硬编码为 100。
   - 处理: 已采纳；在 `main.py` 组合入口显式绑定配置值。
2. 120 ms 预测保持与 100 ms UART 新鲜度不要求相等。
   - 影响: 101-120 ms 的预测目标可以继续显示，但 UART 输出 LOST，主控不会继续使用超出安全窗口的坐标。
   - 证据: `protocol/safety.py` 按 `age_ms` 截止；`MSPM0_WATCHDOG_MS=100`。
   - 处理: 不提高 UART 窗口；补充注释明确为有意安全分层。
3. `GLOBAL_PROPOSAL_WIDTH=320` 会增加丢失态负载属实。
   - 影响: 用户实机旧日志中的全局 target P95 曾达 75-82 ms。
   - 证据: 全分辨率搜索保留 76800 像素；后续已移除该路径的灰度转换与 GaussianBlur，但新实机 P95 尚未返回。
   - 处理: 暂不降回 160；否则会破坏 12×18 至 30×45 像素远距离黑框首捕。
4. `GLOBAL_PROPOSAL_MIN_AREA_RATIO=0.003` 增加小候选属实。
   - 影响: 复杂背景误提议风险提高。
   - 证据: 当前约 230 像素即可进入候选；同时已有宽高比、矩形度、ROI 精炼和三帧确认保护。
   - 处理: 暂不提高；只有实机频繁出现 `CLASSICAL_GLOBAL_REFINE_FAILED` 或错误锁定时再按数据调整。

## 已采纳

- 显式读取并绑定 `MSPM0_STALE_MS`。
- 增加 100/101 ms 协议边界测试。
- 增加视觉预测窗口与 UART 安全窗口分层注释。

## 未采纳

- 不把 UART 新鲜度提高到 120 ms；原因是当前 MSPM0 看门狗为 100 ms。
- 不修改 Pipeline 的通用协议函数签名；ASCII 专用配置应在组合入口绑定，避免破坏其它二参数适配器。
- 不降低全局搜索分辨率或直接提高最小面积；原因是会削弱已验证的远距离小目标能力。

## 验证情况

- 67 个 Python 文件内存编译通过。
- 16 项相关测试和 67 项全量测试通过。
- 未进行 MSPM0 实机看门狗和新全局搜索 P95 验证。

## 依据与工具

- Skill: `C:\Users\Lenovo\.codex\skills\project-logbook\SKILL.md`
- Template: `C:\Users\Lenovo\.codex\skills\project-logbook\references\development-review-log.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\.log\review\2026-07-15_deep-module-review.md`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\main.py`
- Source: `D:\Documents\Desktop\MaixCAM-E-Vision-V2\config.py`
- Tool: `functions.exec -> shell_command`，commands 为源码对照、内存编译及 unittest，cwd `D:\Documents\Desktop\MaixCAM-E-Vision-V2`