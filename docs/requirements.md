# 额度领航员（QuotaPilot）— 需求与实现基线

本文是 `AIQuotaPilot` 仓库的当前产品、设计和验收基线。

## 1. 身份与兼容

- 用户界面名称：**额度领航员**；QuotaPilot 仅作为项目代码名使用。
- 内部 KPlugin ID：`AIQuotaPilot`。为兼容已有安装、配置和钱包条目，本轮不改 ID。
- 平台：KDE Plasma 6、Qt 6.6+、QML、Kirigami、C++ 原生后端。
- Plasma 6 使用 `metadata.json`，不回退到 Plasma 5 的 `metadata.desktop`。

## 2. 当前交付范围

### 已实现

- 多模型、多限额项展示；旧版手动用量与静态限额继续兼容读取。
- MiniMax 原生网络查询、响应校验、地区/端点回退和 KDE Wallet 凭据保存。
- DeepSeek 原生余额查询（`remaining`/`is_available`）、充值金额/充值日期驱动的 PAYG 已用推断；
  CodexZH 原生查询（带 60 秒限流窗口与用量分段）；Agnes AI / Command Code / OpenCode Go
  原生查询（Cookie/API Key 存 KDE Wallet）。
- Codex 采用与 cc-switch 一致的 OpenAI 设备码流程，直接显示结构化验证码并自动打开
  官方浏览器授权页；支持隔离的多账号列表、添加账号、删除账号和默认账号标记。
- Codex 默认账号通过 ChatGPT 官方用量接口读取真实限额窗口；访问令牌临近过期或接口
  返回未授权时自动刷新一次，失败后要求用户重新登录。
- compact 饼图/进度条两种外观，与 popup 图表类型独立。
- 多模型可配置轮询；无数据和错误模型同样参与轮询。
- 会话 D-Bus `ModelActivated(QString)` 事件切换、金色高亮和可配置过期时间。
- compact Tooltip 只展示当前模型第一个限额项，以两行文字描述模型、限额名称、
  已用/总量、单位和重置时间；不在悬浮提示中复制完整面板或图表。
- popup 水平柱状图/环形饼图两种布局，以及关闭、刷新、配置、保持打开。
- 模型增删、上下排序；编辑页所有字段固定为左标签、右输入框。
- 内置 Codex、Claude Code、OpenCode Go、MiniMax、智谱 GLM、Kimi For Coding、
  硅基流动、CodexZH、DeepSeek、Agnes AI、Command Code 十一个厂商预设（目录见
  `providerCatalog.js`），以及自定义模式。
- 固定厂商自动填充稳定标识、名称、官网和套餐结构；只有自定义模式显示限额项与脚本。
- 自定义限额按名称、单位、`${used}` / `${limit}` / `${resetAt}` 变量绑定；支持任意增删。
- HTTP+JS 自定义查询使用独立 worker：提取请求、执行 C++ 网络请求、解析 JSON 响应并
  将返回变量映射为运行时套餐快照。
- 轻量脚本编辑器提供行号、原生语法高亮、原始变量名提示、格式化、安全契约测试、
  自动换行开关和可拖拽高度。
- 供应商新增/编辑通过外层 KCM 的 `saveConfig()` 统一提交当前候选项；Apply/OK 必须先把
  编辑器状态合并到 `cfg_providers`，页面重建后仍能读取刚保存的定义。
- 设置由 KConfig XT 持久化；旧配置自动补齐新字段，旧手动数值不会丢失。

### 尚未实现

- 编辑器中的“测试脚本”目前只做文本契约校验；真实查询在应用设置或刷新后执行。
- Claude Code、智谱 GLM、Kimi For Coding、硅基流动四家固定厂商的凭据管理与真实用量查询适配器；未接入时只显示暂无用量。
- 本地 HTTP 事件服务器。
- D-Bus 服务名/路径/接口的用户自定义。
- 拖拽排序（当前使用可访问的上移/下移按钮）。
- 独立的 `quota-pilot` 图标资产（当前使用 Breeze 系统监控图标）。

这些项不得以占位控件伪装为可用能力。

## 3. 安全与技术决策

- API 请求、Bearer Token 和 KWallet 访问保留在 C++，QML 只读取去敏快照。
- 每个 Codex 账号使用独立、仅当前用户可读写的 Codex 兼容凭据目录；QuotaPilot 只把
  `id_token` 的账号 ID/邮箱声明提供给 QML，不返回、记录或复制任何令牌。
- 任意用户 JavaScript 不在 Plasma 主进程执行。独立 worker 只获得脚本与单次 JSON，
  由父进程实施 CPU/墙钟超时、任务/输出/响应大小上限和 HTTPS 来源策略。
- 本地 HTTP 回调后续使用 Qt HTTP Server 等成熟实现；不在 QML 中扩展 TCP，也不手写
  HTTP 解析器。
- 当前事件入口使用 D-Bus，不监听网络端口。
- 不引入 Qt WebEngine/Monaco。编辑器使用 Qt Quick `TextArea`、C++
  `QSyntaxHighlighter` 和纯文本工具，保持轻量且不获得执行权限。
- MiniMax 等凭据类客户端刷新失败且无旧快照时清空计划、状态为“请求失败”；已有旧快照时保留旧值但
  标记 `stale`，UI 降级灰显并显示“数据暂时不可更新”——不得把旧额度以正常语义色冒充最新数据。
- Codex 请求失败后清空旧计划；生产种子数据不得包含伪造的已用量或限额。
- JSON 整数转换前校验 `qint64` 范围，超范围响应作为无效数据拒绝。
- CodexZH 接口契约要求在 URL query 携带 API key（与 Authorization 头并存），属上游契约风险接受项；
  代码与日志不得打印携带该 key 的完整 URL。

## 4. 数据契约

KConfig 的 `providers` 只保存定义，不保存网络刷新快照：

```typescript
type ProviderDefinition = {
  catalogId: "codex" | "claude-code" | "opencode-go" | "minimax" |
             "zhipu-glm" | "kimi-for-coding" | "siliconflow" | "codexzh" |
             "deepseek" | "agnes-ai" | "command-code" | "custom";
  id: string;
  providerName: string;
  website: string;
  vendor: string;
  sourceLabel: string;
  trustMode: "strict" | "lan" | "custom";
  template: string;
  script: string; // 自定义模式；由独立 worker 执行 request/extractor
  plans: Array<{
    id: string;
    planName: string;
    unit: string;
    sourceType: "native" | "manual" | "http-js";
    usedVariable: string;  // 例如 ${used}
    limitVariable: string; // 例如 ${limit}
    resetVariable: string; // 可选，例如 ${resetAt}，返回可直接展示的时间文本
    manualUsed: number;    // 仅兼容旧配置
    limit: number;         // 仅兼容旧配置
  }>;
};
```

运行时快照只存在内存：

```typescript
type RuntimeProviderSnapshot = {
  providerId: string;
  statusLabel: string;
  errorText: string;
  stale: boolean; // 上次刷新失败但保留旧快照；UI 必须降级灰显，不得以语义色冒充最新
  plans: Array<{
    planId: string;
    planName: string;
    used: number;
    total: number;
    unit: string;
    resetText: string;
    extraText: string;
    isValid: boolean;
    invalidReason: string;
  }>;
};
```

唯一百分比语义为：

```text
usedPercent = clamp(round(used / total * 100), 0, 100)
```

`total <= 0` 或非有限值时为 `-1`。颜色阈值：`<85` 正常、`85..94` 注意、
`>=95` 紧张、无数据灰色。compact 取当前模型所有有效窗口中的最大值。

旧定义缺少 `catalogId` 时，稳定 ID 为 `codex` / `minimax` 的记录迁移到对应预设，
其他记录迁移为自定义；旧 `manualUsed`、`limit` 和 `sourceType` 原样保留。新建自定义
定义使用 `http-js` 契约；刷新会执行真实请求，且不会随机改变真实用量。

## 5. 调度与事件

- `polling`：按配置顺序轮询所有模型，默认 5 秒，范围 1..300 秒。
- `event`：监听会话 D-Bus 路径 `/QuotaPilot`、接口 `org.kde.quotaPilot`、信号
  `ModelActivated(QString)`。
- 参数可为模型稳定 ID 或显示名称。未知或空参数被忽略并记录日志。
- 事件到达时记住原索引、立即切换并高亮；高亮期新事件会刷新计时；到期恢复原索引。
- 默认高亮 30 秒，范围 1..600 秒。

## 6. UI 规则

- compact 单模型直接展示，多模型按当前策略切换；错误覆盖红色感叹号。
- Tooltip 使用原生 `toolTipItem`，由 Plasma 管理悬停时序；内容只取当前模型第一个限额项，
  以文字摘要显示，不绘制进度条，也不列出其余限额项。
- popup 标题为“额度领航员”，四个操作均可键盘聚焦并有可访问名称。
- 水平柱状图始终绘制完整浅色底轨，已用部分叠加语义色；0% 时底轨仍可见。
- popup 底部明确标注“高亮为已使用，灰色为剩余额度”；所有百分比均保持“已使用”语义。
- bar 模式垂直排列模型卡片；pie 模式水平排列模型，每个窗口一个环形图。
- 无数据使用灰色占位，不显示空白；亮暗主题颜色全部来自 Kirigami。
- “保持面板打开”只控制 Plasma popup 失焦行为，不承诺窗口管理器置顶。
- 供应商编辑页使用统一双列布局，基本信息和限额项共享标签宽度与输入列；脚本区撑满正文。
- 配置页只使用外层 KCM 的 OK / Apply / Cancel，不在编辑页增加第二组保存按钮。
- 编辑器提示下拉展示并插入 `used`、`limit`、`resetAt` 等原始名称，不额外包裹 `${}`；
  `${name}` 只用于限额项的变量绑定输入框。
- 固定厂商只显示内置只读信息；自定义显示限额和脚本区。

## 7. KConfig 字段

| 字段 | 默认 | 范围/取值 |
|---|---:|---|
| `compactStyle` | `bar` | `pie` / `bar` |
| `panelStyle` | `bar` | `pie` / `bar` |
| `displayStrategy` | `polling` | `polling` / `event` |
| `pollingIntervalSec` | 5 | 1..300 |
| `eventMode` | `dbus` | 当前仅 `dbus` |
| `highlightDurationSec` | 30 | 1..600 |
| `refreshIntervalSec` | 60 | 10..3600 |
| `opacityPercent` | 80 | 20..100 |
| `keepPanelOpen` | false | bool |
| `sortMode` | `default` | `default` / `alphabetical` / `usedPercent` / `remainingPercent` / `nextReset` / `custom` |
| `customOrder` | 空 | provider id 数组的 JSON 字符串 |
| `popupHeight` / `popupWidth` | 空 | popup 记忆尺寸（运行时写入，非用户编辑） |

## 8. 验收门槛

- `tests/run-static-checks.sh`：所有 QML/JS 测试、qmllint、XML、metadata 和禁用模式检查通过。
- CMake 完整构建通过；C++ parser/client 测试通过。
- `git diff --check` 无空白错误。
- Plasma 桌面会话中运行 `tests/run-plasma-smoke.sh`，检查安装副本、原生插件和运行日志。
- 人工确认 compact、Tooltip、两种 popup 布局、KCM Apply/Cancel、D-Bus 高亮和亮暗主题。
- 新增供应商验收必须覆盖“仍停留在编辑页直接点击外层 Apply/OK”的路径，并在关闭、重开
  配置页以及重启 Plasma 后核对供应商数量、稳定 ID 和脚本摘要。

## 9. 下一阶段顺序

1. 让编辑器“测试脚本”复用定时刷新的真实执行链，并增加自定义凭据的 KDE Wallet 管理。
2. 按厂商逐个增加凭据管理和原生查询适配器，优先 Kimi For Coding 与智谱 GLM，再 Claude Code 与硅基流动。
3. 基于 Qt HTTP Server 增加仅监听 `127.0.0.1` 的回调服务和请求限制。
4. 最后补拖拽排序、独立图标和多实例事件命名空间。
