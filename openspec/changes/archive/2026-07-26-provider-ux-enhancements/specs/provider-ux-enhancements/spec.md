# provider-ux-enhancements — Delta Spec

> 5 项交互/配置增强的契约集合。
> 仓库主 spec 当前为空，本次新增全部能力均落在本 capability 下。

## ADDED Requirements

### Requirement: 手动刷新重置自动刷新计时器

The system MUST reset the auto-refresh interval timer immediately when the user manually triggers a refresh action, so that the next auto-refresh happens `refreshIntervalSec` seconds after the manual refresh, not after the original tick.

#### Scenario: 手动刷新后 Timer 重新计时
- **WHEN** 用户点击 FullView 顶栏的「刷新」按钮或 Plasmoid 上下文菜单的「刷新」动作
- **THEN** `lastRefreshTime` 更新为当前时间，且自动刷新 Timer 立即 stop+restart，新一轮倒计时为 `refreshIntervalSec` 秒

#### Scenario: 定时自动刷新不受影响
- **WHEN** 没有手动触发刷新，且 Timer 自然到期
- **THEN** 仍按 `refreshIntervalSec` 节奏触发 `refresh()`，行为与现状一致

### Requirement: 面板供应商列表可按多种维度排序

The system MUST support sorting the full panel's provider list by selectable modes: `default`（保持配置顺序）/ `alphabetical`（按供应商名首字母 A-Z）/ `usedPercent`（已用%降序，前紧后松）/ `remainingPercent`（剩余%降序）/ `nextReset`（按下次重置时间倒序，最近重置在前）/ `custom`（按用户在配置页中定义的 `customOrder` 数组固定顺序）。

#### Scenario: 排序模式配置生效
- **WHEN** 用户在配置页修改 `sortMode` 字段并保存
- **THEN** FullView 立即重新排序显示，无需重启小部件

#### Scenario: 右上角切换按钮循环切换
- **WHEN** 用户点击 FullView 顶栏右侧的「排序」按钮
- **THEN** `sortMode` 按 `default → alphabetical → usedPercent → remainingPercent → nextReset → custom → default` 顺序循环切换

#### Scenario: 排序不影响 Orb 轮训顺序
- **WHEN** `displayStrategy === "polling"` 时 Orb 自动切换供应商
- **THEN** 轮训顺序按"排序后"的列表顺序进行，下次切换始终指向后一个供应商

### Requirement: 供应商名点击跳转到配置的官网

The system MUST make the provider name in `ProviderGroup` clickable when the provider has a `website` URL configured, and opening it MUST launch the system default browser.

#### Scenario: 配置官网后点击打开浏览器
- **WHEN** 用户在配置页输入 `providerName.website` 为合法 URL（如 `https://example.com`）并保存
- **THEN** FullView 中该供应商名变为可点击链接，cursor 变为 pointer；点击后调用 `Qt.openUrlExternally(website)` 打开系统默认浏览器

#### Scenario: 未配置官网时不可点击
- **WHEN** 供应商 `website` 为空字符串或不是以 `http://` / `https://` 开头的 URL
- **THEN** 供应商名保持普通 Label 样式，无 cursor 变化，无点击响应

### Requirement: Orb 切换动画优雅无闪烁

The system MUST render the Orb provider switch with a smooth cross animation that does NOT exhibit a visible blank/black-frame intermediate state and MUST NOT cause the percentage label to flicker.

#### Scenario: 轮训切换流畅
- **WHEN** Orb 切换到下一个供应商时
- **THEN** 在 ≤ 200ms 内完成新供应商的渲染进场；进度条数值与百分比文字与新供应商数据一致，无中间空白帧

#### Scenario: 颜色主题切换正常
- **WHEN** 切到的供应商已用%从绿色变化到黄色/红色时
- **THEN** 颜色渐变过渡自然，没有瞬切；百分比数字保持稳定

### Requirement: 供应商配置页支持 Logo 选择与启用/禁用

The system MUST let users configure a logo and an enabled/disabled toggle for each provider in the configuration page. Built-in providers (MiniMax / Codex / GLM / 云之声等) MUST ship with embedded logo SVG strings; custom providers MUST support a user-supplied local file path. Disabled providers MUST NOT appear in the FullView panel and MUST NOT participate in Orb polling.

#### Scenario: 内置供应商默认显示内置 Logo
- **WHEN** 用户添加或启用一个内置 catalog 的供应商（如 MiniMax）且未自定义 Logo
- **THEN** FullView 中该供应商卡片左侧显示 catalog 自带的 logo SVG

#### Scenario: 自定义供应商配置本地路径
- **WHEN** 用户在配置页的 `providerName.logoPath` 字段填入一个本地绝对路径（如 `file:///home/user/.local/share/icons/my.png`）或相对路径
- **THEN** FullView 中该供应商卡片左侧显示该路径对应的图片（按 24x24 圆形遮罩）；路径无效（不存在/无权限/非图片）时降级显示首字符

#### Scenario: 未配置 Logo 回退到首字符
- **WHEN** 供应商既无内置 logo 也无 `logoPath`
- **THEN** FullView 卡片左侧显示供应商名首字符的圆角矩形（与现状一致）

#### Scenario: 禁用供应商不显示在面板
- **WHEN** 用户在配置页设置 `providerName.enabled = false`
- **THEN** FullView 列表中不再包含该供应商；Orb 轮训顺序也跳过该供应商

#### Scenario: 重新启用恢复显示
- **WHEN** 用户将 `providerName.enabled` 切回 `true`
- **THEN** 面板立即重新包含该供应商，恢复显示与轮训

#### Scenario: 禁用状态下 Orb 仍可工作
- **WHEN** 所有供应商均被禁用
- **THEN** FullView 显示「暂无供应商数据」占位；Orb 显示 `—` 状态

### Requirement: 数据来源仅为真实 extractor，不使用 mock 数据

The system MUST NOT generate or display any random/mock/synthesized usage data. All visible numbers MUST come from real extractor calls (MiniMax API / Codex API / user-provided script) or from default seed values defined in `providerRegistry.js` when the extractor has not yet been configured.

#### Scenario: 未配置 extractor 时显示提示
- **WHEN** 一台供应商缺凭证（如 MiniMax / Codex 未保存 API Key）或 custom provider 尚未提供脚本
- **THEN** PanelList 显示该供应商的 statusLabel = "未配置" / "未登录" / "暂无脚本"，不显示任何百分比数字

#### Scenario: extractor 失败时显示错误状态
- **WHEN** refreshOne(providerId) 抛出错误或返回 snapshot 带的 errorText
- **THEN** ProviderGroup 显示 errorText 红色 banner；Orb 切到该供应商时仍按 tightest 已用%（若可用）显示 LED，不显示随机数字

### Requirement: 内置供应商 extractor 接入

The system MUST register all built-in providers (MiniMax / Codex / GLM / 云之声 / Claude / Gemini / GPT) in `providerRegistry.js` with their logo SVG, website, and default configuration. The system MUST implement real extractor integration for MiniMax, Codex, and custom-script providers. For the remaining built-in providers, the system MUST display "未配置" status and NOT synthesize any usage numbers.

#### Scenario: MiniMax 真实额度接入
- **WHEN** 用户在配置页保存 MiniMax API Key 到 KDE 钱包且 refreshOne("minimax") 被调用
- **THEN** 返回的 snapshot 包含真实使用计划与限额，ProviderGroup 正确显示

#### Scenario: Codex 真实额度接入
- **WHEN** 用户完成 Codex OAuth 登录且 refreshOne("codex") 被调用
- **THEN** 返回的 snapshot 包含真实 codex usage，ProviderGroup 正确显示

#### Scenario: 自定义脚本供应商真实接入
- **WHEN** 用户在配置页提供 custom script 且 refreshOne("custom-xxx") 被调用
- **THEN** 脚本在独立进程中执行（沿用 cc-switch 的 request/extractor 模式），返回 JSON snapshot 后显示

#### Scenario: 未实现 extractor 的内置供应商占位
- **WHEN** displayProviders 中含 catalogId 为 "glm" / "claude" / "gemini" / "gpt" 等本期未实现 extractor 的供应商
- **THEN** statusLabel 固定为 "未配置"，plans 为空，不显示任何使用率
