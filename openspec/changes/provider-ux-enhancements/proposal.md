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
