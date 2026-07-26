# Comet Design Handoff

- Change: provider-ux-enhancements
- Phase: design
- Mode: compact
- Context hash: cb02954baf06312847d8db750315aa86b8214c6f42ff75cc9c7f942f8543a046

Generated-by: comet-handoff.sh

OpenSpec remains the canonical capability spec. This handoff is a deterministic, source-traceable context pack, not an agent-authored summary.

## openspec/changes/provider-ux-enhancements/proposal.md

- Source: openspec/changes/provider-ux-enhancements/proposal.md
- Lines: 1-51
- SHA256: 9db57ed88f9f1fa827a7cfc14bf85748f6ff13c61200262f706f8d5c62b3509b

```md
## Why

AIQuotaPilot 当前的悬浮面板列表已能满足"看到额度"这一基本需求，但随着多供应商接入（MiniMax / Codex / 云之声 / 自定义脚本），用户在以下五个场景存在明显卡点：

1. 手动点击"刷新"后，60s 自动刷新 Timer 仍按原节奏触发，刚刚发起的手动请求被定时器覆盖，体验上等于"刷新按钮白按了"。
2. 面板供应商顺序写死在 `mockData.js` 的 `SEED_PROVIDER_DEFINITIONS` 数组里，无法按"已用%""下次重置"等更有用维度排序，也没有"快速切换排序"的入口。
3. 看到额度紧张想跳到官网续费时，需要手动复制供应商名去浏览器搜索，没有一键跳转。
4. Orb 在多供应商轮训时，`CompactView` 的 `providerSwitch` 动画把整个组件淡出再淡入，百分比数字因此"跳一下"，视觉上不够优雅。
5. 自定义供应商越来越多，配置页缺乏 Logo 区分，也没有"暂不订阅先禁用"的开关 — 暂不订阅的供应商仍占着面板槽位和 Orb 轮训名额。

本次变更把这 5 项统一为 `provider-ux-enhancements` 一次性交付。

## What Changes

- **新增 capability `provider-ux-enhancements`**：覆盖 5 项交互/配置增强；`mockData.js` 扩展显示模型字段（`resetTimestamp`、`enabled`、`logoPath`），`FullView.qml` / `CompactView.qml` / `ProviderGroup.qml` 增加对应交互，`config/main.xml` + `ProvidersConfig.qml` + `ProviderEditor.qml` 增补 Logo 选择 + 启用/禁用开关 + 排序下拉。
- **手动刷新重置计时器**：`main.qml` 增加 `refreshTimer` 显式 `Timer` 对象；`refresh()` 调用时先 stop+restart 计时器，避免 60s 内被定时器再次触发。
- **面板排序可配置**：
  - `Plasmoid.configuration.sortMode` 新增字符串字段（`default` / `alphabetical` / `usedPercent` / `remainingPercent` / `nextReset` / `custom`）。
  - `FullView.qml` 右上角新增 ToolButton 切换按钮（图标 `view-sort`），点击循环切换 `sortMode`。
  - `mockData.js` 新增 `sortProviders(displayProviders, definitions, mode)` 函数；`main.qml` 的 `providers` 改为派生属性，调用 `sortProviders` 后渲染。
  - 面板右上角按钮显示当前模式 tooltip，便于用户确认。
- **供应商名点击跳转官网**：`ProviderGroup.qml` 把 `providerNameLabel` 改为 MouseArea + `Kirigami.Link` 风格；`onClicked` 调 `Qt.openUrlExternally(website)`。未配置 `website` 时回退为普通 Label。
- **Orb 切换动画优化**：`CompactView.qml` 的 `providerSwitch` 替换为短时长（120ms）`NumberAnimation` 透明度 + `ScaleAnimator` 0.92→1 缩放交叉动画；保留颜色但去掉"全黑屏"中间态。
- **Logo + 启用/禁用**：
  - `providerCatalog.js` 内置固定供应商（MiniMax / Codex / GLM / 云之声）的 logoBase64 数据（SVG 字符串）。
  - 自定义供应商支持 `logoPath` 字符串字段（绝对路径或 `file://` URL）。
  - `ProviderGroup.qml` 标题左侧显示 24x24 Logo 图标（`Image` 或 fallback 到首字符圆角矩形）。
  - `ProvidersConfig.qml` 每个供应商卡片增加"启用/禁用"开关（`enabled` 字段，默认 true）。
  - `providers` 列表派生时，`enabled === false` 的供应商在面板和 Orb 轮训中均被过滤。

无 **BREAKING** 改动：旧配置缺字段时按 `enabled=true` / `logoPath=""` / `sortMode="default"` 兜底。

## Capabilities

### New Capabilities

- `provider-ux-enhancements`: 5 项交互/配置增强的契约集合，包括面板排序可配置、供应商名点击跳转官网、Orb 切换动画优雅化、Logo 配置与启用/禁用、以及手动刷新重置计时器。

### Modified Capabilities

无（仓库尚无主 spec，本次新增的全部能力均落在 `provider-ux-enhancements` 一个新 capability 下）。

## Impact

- **QML UI**：`package/contents/ui/main.qml`、`CompactView.qml`、`FullView.qml`、`ProviderGroup.qml`、`ProviderPieView.qml`（若有引用）、`config/ProvidersConfig.qml`、`config/ProviderEditor.qml`、`config/GeneralConfig.qml`。
- **逻辑 JS**：`package/contents/js/mockData.js`（显示模型字段扩展 + 排序函数）、`package/contents/js/providerCatalog.js`（内置 logo 字典）。
- **配置**：`package/contents/config/main.xml`（新增 `sortMode`，必要时补 `enabled` / `logoPath` 在 providers JSON 序列化层）。
- **文档**：`docs/requirements.md` 增量（验收清单补 5 项）、`docs/usage-script-spec.md` 补 `nextResetAt` 字段建议（可选）。
- **数据兼容性**：旧 Plasmoid configuration JSON 中 `providers` 数组缺 `enabled` / `logoPath` 时，由 `normalizeDefinitions` 兜底填充。
- **测试**：`mockData.js` 纯函数排序逻辑可单测；QML 端用 `plasmawindowed` 跑视觉验收。
- **无新增依赖**，无 C++ 改动。
```

## openspec/changes/provider-ux-enhancements/design.md

- Source: openspec/changes/provider-ux-enhancements/design.md
- Lines: 1-94
- SHA256: 32fdb010124dc6331df4a3c055800c2a5e09bdd90070317bfb1bd6333a9c9047

[TRUNCATED]

```md
## Context

AIQuotaPilot 当前架构：

```
package/contents/ui/main.qml              PlasmoidItem (Timer × 3 + Connections + CompactView + FullView)
package/contents/ui/CompactView.qml       Orb（compactRepresentation）：PieChart/Bar + providerSwitch 动画
package/contents/ui/FullView.qml          面板（fullRepresentation）：header(H4 + 4 个 ToolButton) + ListView(ProviderGroup) + statusLabel
package/contents/ui/ProviderGroup.qml     供应商卡片：标题 + LED + Plans Repeater
package/contents/ui/ProviderEditor.qml    配置：基本信息 + 限额项 + 脚本编辑器
package/contents/js/mockData.js           显示模型派生 + 颜色/排序辅助
package/contents/js/providerCatalog.js    内置供应商定义（minimax / codex / …）
package/contents/config/main.xml          KConfig XT 配置存储
package/contents/config/config.qml        ConfigModel 入口（常规 / 供应商）
package/contents/config/ProvidersConfig.qml  供应商列表编辑
package/contents/config/GeneralConfig.qml    常规面板
docs/requirements.md                      需求基线
docs/usage-script-spec.md                 自定义脚本规范
```

约束：
- QML-only，无 C++ 编译
- Plasmoid configuration 通过 `Plasmoid.configuration.<key>` 读写，`providers` 是 JSON 字符串
- 旧配置可能缺 `enabled` / `logoPath` / `sortMode` 字段，必须兼容
- `docs/usage-script-spec.md` 已规定 `resetAt` 字段，无需改动

## Goals / Non-Goals

**Goals**
- 5 项功能一次性交付并通过 `plasmawindowed` 视觉验收
- 排序逻辑放在 `mockData.js` 纯函数中，便于在脑外/脚本测试
- 状态字段（`enabled` / `logoPath` / `sortMode`）在 `normalizeDefinitions` 与新读取层兜底
- 不引入 C++ 改动、不引入新依赖

**Non-Goals**
- 不做"按曲线分组"
- 不做云端 Logo 同步
- 不做集成测试（Plasma 6 单元测试框架繁重，本次只做 `mockData.js` 单测 + 视觉验收）
- 不动 `usage-script-spec.md` 已有的 `resetAt` 约定

## Decisions

### D1. 排序放在 `mockData.js` 纯函数
**为什么**：`mockData.js` 是 `.pragma library`，不能持有 QML 状态；纯函数 + QML 端 `property var sortedProviders: MockData.sortProviders(providers, ..., sortMode)` 派生即可。
**备选**：把排序放在 `main.qml` JS 内 → 拒绝，混入 QML 上下文，不利于单测。

### D2. `nextResetAt` 字段在 `displayPlan` 层补齐
**为什么**：脚本规范已允许 `resetVariable` 提取重置时间，但 mock 阶段没有；为支持按"nextReset"排序，必须在 `_displayPlan` 中补 `nextResetAt`（毫秒时间戳；不可用时为 -1 表示"未排"）。
**mock 阶段估算**：根据 `plan.id`（如 `five-hours` / `seven-days` / `thirty-days`）推断周期，加上当前时间得到下次重置 ≈ `now + 周期`。
**备选**：要求 user 显式配置 `resetText` 解析 → 拒绝，不友好。

### D3. Orb 切换动画用 `CrossFade` + 缩放，不用 `Opacity` 全黑过渡
**为什么**：`CrossFade` 一帧内两个 Item 同时半透明叠加，肉眼几乎无黑屏；保留 `ScaleAnimator` 0.92→1 缩放带来轻微"入场感"。
**备选 A**：继续用 `Opacity` 0→1 → 拒绝，仍有黑屏。
**备选 B**：直接换 `Charts.PieChart` fast-binding 复用同一组件 → 拒绝，无法避免 push 弹入感。

### D4. Logo 字段双轨：内置 SVG base64 + 本地路径
**为什么**：内置供应商（MiniMax / Codex / GLM / 云之声 / Gemini / Claude）logo 写死在 `providerCatalog.js`；自定义供应商经配置页输入 `logoPath`（绝对路径或 `file://` URL）。
**QML 端**：`Image` 组件先尝试 `source = logoPath`，失败回退到 `Text(firstChar)` 圆角矩形，`Image.status` 监听 `Error` 切回退。
**为什么不用 `QIcon::fromTheme`**：本项目刻意不引入额外的图标主题依赖。

### D5. 启用/禁用过滤在显示模型派生阶段完成
**为什么**：`buildDisplayProviders` 之后立即调用 `filterEnabled(definitions, snapshots)`，下游不直接见到禁用供应商；这样 `highlightTimer` / `nextProviderIndexWithUsage` / `eventHighlighted` 都自然不引用禁用供应商。
**备选**：在 `ProviderGroup` 层 `visible: enabled` 隐藏 → 拒绝，仍占 ListView 槽位且 Orb 轮训会卡住。

### D6. 官网跳转用 `Qt.openUrlExternally`
**为什么**：Plasma 6 QML 标准 API；等价于 `xdg-open`，沙箱/Applet 上下文可用。
**安全**：仅在 `website` 通过 `^https?://` 正则校验后才调用，避免 `javascript:` / `file://` 注入。

### D7. 手动刷新重置 Timer 用 `Timer.restart()`
**为什么**：QML `Timer` 有原生 `restart()` 方法，省去手动 stop+start。
**改动**：`main.qml` 把匿名 Timer 改为 `id: refreshTimer` 暴露；`refresh()` 内调用 `refreshTimer.restart()`。

## Risks / Trade-offs

- [R1] `Qt.openUrlExternally` 在 Plasma sandbox 不可用 → 已在 Plasma 6.x 默认放行；如失败保留 tooltip 提示。
- [R2] 用户输入 `logoPath` 路径无效导致 `Image` 频繁出错 → 接 `sourceChanged` 一次性回调，缓存 fallback 状态，避免反复触发。
- [R3] 排序模式 `custom` 依赖用户维护 `customOrder` 数组 → 当用户删了某个供应商，customOrder 自动剔除保留项；新建时可拖拽或下拉追加（v2 给出 placeholder）。
- [R4] 动画 `CrossFade` 在低帧率设备上仍可能不流畅 → 时长上限 200ms，可观察。
- [R5] 5 项功能交叉修改 `providers` JSON 字段 → `normalizeDefinitions` 兜底必须覆盖所有缺字段路径；首次 `git checkout` 旧配置的用户应能平滑升级。
```

Full source: openspec/changes/provider-ux-enhancements/design.md

## openspec/changes/provider-ux-enhancements/tasks.md

- Source: openspec/changes/provider-ux-enhancements/tasks.md
- Lines: 1-71
- SHA256: 40c36f0fa57e4a266ad4b086a1036e814637f70e09e82cd8529c556f144203d2

```md
# provider-ux-enhancements — Tasks

## 1. mockData.js 数据扩展

- [ ] 1.1 `normalizeDefinitions` 末尾对每个 definition 兜底补 `enabled = true`、`logoPath = ""`、`customOrder = null` 字段
- [ ] 1.2 `_displayPlan` 返回值补 `nextResetAt`（毫秒时间戳，mock 阶段根据 plan.id 周期估算；不可用时 -1）
- [ ] 1.3 新增 `filterEnabled(definitions, snapshots)` 纯函数，返回已过滤的 `{definitions, snapshots}`
- [ ] 1.4 新增 `sortProviders(displayProviders, mode, customOrder)` 纯函数，支持 `default` / `alphabetical` / `usedPercent` / `remainingPercent` / `nextReset` / `custom`
- [ ] 1.5 新增 `firstCharFallback(name)` 辅助（中文取首字、英文取首字符，统一转大写）

## 2. providerCatalog.js 内置 Logo 字典

- [ ] 2.1 为 `minimax` / `codex` / `glm` / `cloud-voice` / `gemini` / `claude` / `gpt` 注册 `logoSvg` 字段（base64 或内联 SVG 字符串）
- [ ] 2.2 在 `providerOptions()` 输出中暴露 `logoSvg` 字段供配置页预览
- [ ] 2.3 若内置供应商缺 `website`，补全为各官方主页（MiniMax / Codex / 等）

## 3. main.qml 排序派生 + 计时器重置

- [ ] 3.1 `providers` 改为派生属性：`filterEnabled` + `sortProviders` 串起来
- [ ] 3.2 匿名 Timer 改成 `id: refreshTimer`，`refresh()` 末尾调用 `refreshTimer.restart()`
- [ ] 3.3 `sortMode` 读取 `Plasmoid.configuration.sortMode || "default"`，写入 `Plasmoid.configuration.sortMode` 由 FullView 按钮触发
- [ ] 3.4 `customOrder` 读取 `Plasmoid.configuration.customOrder || ""` 字符串（JSON 数组），变更时回写

## 4. FullView.qml 排序按钮 + 状态栏

- [ ] 4.1 headerActions 增加"排序"ToolButton（图标 `view-sort`），点击循环切换 `sortMode` 并写回配置
- [ ] 4.2 ToolButton 动态 Accessible.name / ToolTip.text 反映当前模式
- [ ] 4.3 `statusLabel` 末尾加 ` · 排序：<mode>` 一段

## 5. ProviderGroup.qml 官网跳转 + Logo 显示

- [ ] 5.1 `providerNameLabel` 改为可点击 MouseArea：仅当 `website` 通过 `https?://` 正则校验时启用，提供 hover cursor 变化与 `Qt.openUrlExternally(website)` 信号
- [ ] 5.2 标题 Row 左侧增加 24x24 Logo 槽：`Image` 加载 `logoPath` 或 `logoSvg`；失败时 fallback 到 `Text(firstChar)` 圆角矩形
- [ ] 5.3 接收 `logoSource` / `logoChar` 两个新属性；`enabled` 字段供上层过滤后不再由 ProviderGroup 自行处理

## 6. CompactView.qml Orb 切换动画

- [ ] 6.1 现有 `providerSwitch` SequentialAnimation 替换为 `CrossFade` + `ScaleAnimator`
- [ ] 6.2 缩放起始 0.94、时长 120ms、缓动 `OutCubic`
- [ ] 6.3 移除整组件 `opacity` 1→0→1 切换；保留颜色由 `usageColor` 一次绑定

## 7. ProvidersConfig.qml 启用/禁用 UI

- [ ] 7.1 每个供应商列表项增加"启用/禁用"Switch（绑定 `providerObj.enabled`），默认 true
- [ ] 7.2 列表渲染前按 `enabled` 过滤（仅 UI 层过滤，运行时仍由 mockData.filterEnabled 兜底）
- [ ] 7.3 列表项增加 Logo 缩略图（与 ProviderGroup 展示一致）

## 8. ProviderEditor.qml Logo 路径输入

- [ ] 8.1 基本信息区增加 `logoPath` 输入框（Kirigami.TextField，含 placeholder `file://...`）
- [ ] 8.2 输入变更调 `updateField("logoPath", value)`
- [ ] 8.3 输入框左侧 24x24 预览：内置 catalog 直接显示 `logoSvg`，自定义根据 `logoPath` 加载

## 9. config/main.xml 新增字段

- [ ] 9.1 group `ui` 增加 `sortMode`（string，默认 `default`）
- [ ] 9.2 group `ui` 增加 `customOrder`（string，默认空）—— 序列化 JSON 数组

## 10. 视觉验收

- [ ] 10.1 `plasmawindowed aiUsageWatcher` 启动；手工验证 5 项验收场景
- [ ] 10.2 对比改前/改后动画：Orb 切换无明显黑屏闪烁
- [ ] 10.3 配置页改 sortMode 后面板立即重排
- [ ] 10.4 禁用某个供应商后，面板 + Orb 轮训均跳过
- [ ] 10.5 给自定义供应商配 logoPath 后面板显示对应图片

## 11. 回归

- [ ] 11.1 旧配置（无 `enabled` / `logoPath` / `sortMode` 字段）首次加载行为不变
- [ ] 11.2 `kpackagetool6 --install` 安装到本地 Plasma 目录跑通
- [ ] 11.3 `git diff` 对照 proposal/design，检查无多余改动
```

## openspec/changes/provider-ux-enhancements/specs/provider-ux-enhancements/spec.md

- Source: openspec/changes/provider-ux-enhancements/specs/provider-ux-enhancements/spec.md
- Lines: 1-118
- SHA256: d8bd87f591b2813a688c8f4420208779ee8f43d4926fbfc990098a41c251e177

[TRUNCATED]

```md
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
```

Full source: openspec/changes/provider-ux-enhancements/specs/provider-ux-enhancements/spec.md

