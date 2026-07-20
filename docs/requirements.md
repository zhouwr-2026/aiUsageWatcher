# AI Usage Watcher — 需求汇总

> 本文档是 aiUsageWatcher 的**核心需求基线**。
> 数据来源：
> 1. 上一轮 `ai-desktop-pet` 项目的需求文档（docs/requirements.md + docs/superpowers/specs/2026-07-19-usage-monitor-handoff.md）
> 2. 本轮 KDE Plasma 6 重写期间用户累积的所有要求
> 3. KDE 开发者站（develop.kde.org）Plasma 6 小部件规范
>
> 任何冲突以此文档为准。

## 1. 项目目标

KDE Plasma 6 桌面小部件，**实时监控各大模型厂家的模型套餐用量**。常驻桌面 / 面板，单击展开为弹出框。

### 1.1 形式（KDE Plasma 6 小部件）

- 安装目录：`~/.local/share/plasma/plasmoids/aiUsageWatcher/`
- 包结构：`package/metadata.json` + `package/contents/{ui,config}/`
- 根 QML：`PlasmoidItem`（Plasma 6 强制要求）
- 运行命令（开发期）：`plasmawindowed aiUsageWatcher`
- 安装命令：`kpackagetool6 --install aiUsageWatcher`
- 卸载：`kpackagetool6 --remove aiUsageWatcher`
- Plasma API 版本：`X-Plasma-API-Minimum-Version: "6.0"`
- 包结构：`KPackageStructure: "Plasma/Applet"`

### 1.2 视觉规格（仿 KDE 磁盘使用率小部件 + KDE 顶部时间小部件）

| 状态 | 表现 |
|---|---|
| compact（面板/小尺寸） | 圆球显示最紧张计划的"已用 %"；单击展开 |
| full（弹出/大尺寸） | 工具栏 + **每供应商一段**的卡片组；每段内**每限额一条水平进度条** |

**full 视图布局**（仿 KDE `org.kde.plasma.diskusage`）：
```
┌─────────────────────────────────────────────┐
│ 模型用量                  [配置] [固定]      │  ← RowLayout 工具栏
├─────────────────────────────────────────────┤
│ ┌─────────────────────────────────────────┐ │
│ │ ● 云之声Token Hub            自定义 可用  │ │  ← ProviderGroup
│ │   5小时   ████████░░  65%   重置 今天 18:00│ │     PlanBar
│ │   7天     ████░░░░░░  22%   重置 周日 00:00│ │
│ │   30天    █░░░░░░░░░   8%                 │ │
│ └─────────────────────────────────────────┘ │
│ ┌─────────────────────────────────────────┐ │
│ │ ● MiniMax                套餐 降级          │ │
│ │   余额    █████████░  88%   活动期 8 月底结束 │ │
│ └─────────────────────────────────────────┘ │
│ ┌─────────────────────────────────────────┐ │
│ │ ● 云知声 Token Hub         自定义 异常       │ │
│ │   脚本解析失败：unexpected token at pos 32 │ │
│ └─────────────────────────────────────────┘ │
└─────────────────────────────────────────────┘
```

**进度条配色语义**（按已用 %）：
- `usedPercent ≤ 5%` → 红 `#f87171`
- `5% < usedPercent ≤ 15%` → 黄 `#fbbf24`
- `usedPercent > 15%` → 绿 `#34d399`
- 无数据 / 无 total → 灰 `#6b7280`，显"—"

**卡片外壳着色**（边框 + LED 灯）：与最紧张计划同色
- 整体供应商外壳颜色也按最紧的 plan 走
- 没有任何 plan 时（如脚本解析失败）走 LED 红色 + 错误文本

**每条 PlanBar** 的标准字段（从 DisplayPlan 取）：
- `planName` 计划名（如 "5小时"、"7天"、"余额"）
- `usedPercent` 0–100
- `usedPercentLabel` "29%"
- `barClass` bar-green / bar-yellow / bar-red / bar-gray
- `resetText` "今天 18:00"（无 reset_at 时整行隐藏）
- `usedText` "141775516 / 180000000"（脚本返回 used+total 时显示）
- `unitText` "$" / "tokens" / "%"
- `extraText` 自由补充文本（活动期等）

**通用 UI 要求**：
- 弹出框窗口：透明背景、圆角、半透明深色 (`rgba(10,14,26,0.80)`)
- 进度条圆角、过渡动画 300ms（easeOutCubic）
- 不要系统任务栏图标（小部件本身就是桌面常驻，不重复登记任务栏）

## 2. 数据来源（多供应商）

### 2.1 通用 JavaScript 用量脚本
- 自定义 HTTP 请求 + extractor 函数
- 见 [docs/usage-script-spec.md](usage-script-spec.md) 完整规范
- 必须实现：URL/方法/请求头/Body 构造；响应解析；占位符替换（`{{baseUrl}}` `{{apiKey}}` `{{accessToken}}` `{{userId}}`）

### 2.2 余额型（balance）

参考 `cc-switch-main` 中已支持：
- DeepSeek
- StepFun（阶跃星辰）
- SiliconFlow（国内/国际）
- OpenRouter
- Novita AI

### 2.3 套餐型（token_plan / coding_plan）
- Kimi（api.kimi.com）
- 智谱 / 智谱团队版
- MiniMax
- ZenMux
- 火山方舟

### 2.4 官方订阅（official_subscription）
- Claude（本机 OAuth 登录）
- Codex
- Gemini

### 2.5 GitHub Copilot（可选）
- 仅在用户启用 GitHub Copilot 配置时启用
- 复用官方 Copilot API

### 2.6 用户自定义扩展
- 用户可通过"配置"页添加/编辑/删除供应商
- 每个供应商：名称、URL、API Key/Token、用法脚本、信任模式（Strict / Lan / Custom）

## 3. extractor 返回字段（extractor return shape）

> 权威规范见 [docs/usage-script-spec.md](usage-script-spec.md) 第二节。
> 所有字段**均为可选**。

| 字段 | 类型 | 含义 |
|---|---|---|
| `planName` | string | 套餐名（如 "5小时窗口" / "7天窗口" / "余额"） |
| `remaining` | number | 剩余额度 |
| `used` | number | 已用额度 |
| `total` | number | 总额度 |
| `unit` | string | 单位（短字符串：≤8 字符且不含空白） |
| `isValid` | boolean | 套餐是否有效 |
| `invalidMessage` | string | 失效原因 |
| `resetAt` | string | 重置时间点 |
| `extra` | string | **自由补充要展示的文本**（活动期、计费说明等） |

## 4. 安全要求（继承自 ai-desktop-pet）

### 4.1 脚本沙箱
- QuickJS 沙箱执行
- 内存限制 16 MB，超时 400 ms

### 4.2 SSRF 防护
- 禁止：链路本地（169.254/16、fe80::/10）、多播、未指定、云元数据地址（169.254.169.254 等）
- Strict 模式：仅 https + 回环（域名解析后）
- Lan 模式：仅私网/回环（需用户显式加入批准列表）
- Custom 模式：可访问任意公网

### 4.3 凭据隔离
- 密钥仅存储于 **KDE Wallet**（替代原 libsecret/system keyring）
- 预览/测试快照/日志/IPC 永远不返回真实密钥
- 占位符替换使用**哨兵值**：JS 解析前用 `__usage_api_<32hex>__` 替换，HTTP 实际请求前才替换为真值
- 占位符仅允许在单/双引号字符串内、查询参数值、请求头值、请求体内

### 4.4 响应安全
- 响应解析前过滤掉回显密钥（避免回声攻击）
- 响应大小限制 256 KiB
- 转义码检查（含 `\uXXXX` 解码后比较）

## 5. 渲染层（核心架构要求）

### 5.1 单一展示对象
- **所有**来源 → 归一化为同一 `DisplayQuota` 对象
- QML / UI 只读这个对象
- 转换函数：`toDisplayQuota(provider)`，内部完成：百分比计算、状态色阈值映射、单位是否自由文本识别、姓名后缀剥除、重置时间点收集

### 5.2 关键字段
- `providerName`：自动剥 ` · Claude/Codex/OpenCode` 后缀（seed 误拼的）
- `unit`：超 8 字符或含空白 → 改放 `unitOverflow` 单独展示
- `usedPercent` / `usedPercentLabel`：主圆球指标
- `percent` / `percentLabel`：备用（剩余 %）
- `resetTimes: ResetEntry[]`：只保留 `reset_at` 存在的计划

## 6. 数据后端

### 6.1 配置存储
- 非敏感配置：KConfig（`~/.config/aiusagewatcherrc` 或 KConfig XT）
- 密钥：KDE Wallet（`kwallet6`）
- 数据库：本地 SQLite 缓存历史用量（可选）

### 6.2 并发与缓存
- 有界并发（同时最多 4 个请求）
- 24h 缓存回退：瞬时错误（5xx / 网络）用最近成功数据降级显示
- 永久错误（4xx 鉴权 / 解析错误）清缓存，显示错误

## 7. 交互

| 入口 | 行为 |
|---|---|
| 桌面 compact 视图单击 | 切换 `plasmoid.expanded` |
| 弹出框外点击 / 失焦 | 关闭弹出框（**默认 200ms 延迟**，便于鼠标移入） |
| **配置** 按钮 | 打开 KCM 配置窗口（`aiusagewatcher` KCM module） |
| **固定** 按钮 | 切换 popup 的 `alwaysOnTop` + 取消自动关闭逻辑 |
| 弹框内"重置时间点"分组 | 只展示有 `reset_at` 的计划 |

## 8. 性能与限制

- 默认刷新间隔：60 s（可在 KCM 配置）
- 默认并发上限：4
- 默认缓存 TTL：24 h
- 默认响应体上限：256 KiB
- 默认 JS 超时：400 ms
- 默认 HTTP 超时：10 s

## 9. 验收清单

### 9.1 功能

- [ ] `plasmawindowed aiUsageWatcher` 能启动并在桌面显示 compact 圆球
- [ ] 单击 compact → 展开 full 视图
- [ ] full 视图显示每个供应商的：套餐名、已用 %、剩余/总量、重置时间点、extra 文本
- [ ] full 视图右上角两个按钮（配置 / 固定）功能可用
- [ ] KCM 配置窗口可添加/编辑/删除供应商
- [ ] 至少 1 个 `cc-switch` 内置 provider（token_plan/balance/subscription）跑通
- [ ] 至少 1 个自定义 JavaScript 脚本跑通

### 9.2 安全

- [ ] 真实密钥不出现在任何日志/快照/IPC/前端状态/截图
- [ ] SSRF 防护：所有 dangerous IP / metadata 地址被拒绝
- [ ] 占位符仅允许出现在合法位置
- [ ] 响应解析时剔除回显密钥
- [ ] JS 沙箱超时与内存限制生效

### 9.3 UI / 可用性

- [ ] compact 圆球颜色随最紧张 plan 的已用 % 阈值切换（绿/黄/红/灰）
- [ ] full 视图按"每供应商一段、每限额一条 bar"渲染
- [ ] PlanBar 颜色随该计划的已用 % 阈值切换
- [ ] ProviderGroup 边框 / LED 灯颜色按该供应商最紧张 plan 走
- [ ] 没有 7 天 reset 的供应商不显示空行
- [ ] `unit` 长文本不撑爆数字，单独行展示
- [ ] 供应商名自动剥 ` · <App>` 后缀
- [ ] 80% 半透明深色背景，圆角
- [ ] 进度条填充动画 ≥ 300ms（easeOutCubic）
- [ ] QML 通过 `qmllint` 无错误

## 10. 非目标（不在本期范围）

- 语音对话
- 移动端（Plasma Mobile 暂不在范围）
- 多用户/团队协作
- 云端同步
- 账号管理（仅本地 KDE Wallet）
- 第三方同步 UI（用户明确否决：不要 cc-switch 同步/导入向导 UI）

## 11. 开放问题（待开始实现时确认）

1. Plasma 6 在 Wayland 下小部件是否仍受 alwaysOnTop / skipTaskbar hint 影响，还是 Containment 自动处理？
2. KWallet 在 Plasma 6 的标准调用模式是 `org.kde.KWallet` D-Bus 接口 vs `KWallet` C++ 类？
3. QuickJS 在 QML 上下文的执行：plasmoid 内 JS 还是 separate C++ backend？
4. 实时刷新的 Timer 由 QML 提供还是 C++ backend 提供？