---
comet_change: provider-ux-enhancements
role: technical-design
canonical_spec: openspec
---

# provider-ux-enhancements — Technical Design

## Context

AIQuotaPilot 是 Plasma 6 QML-only 桌面小部件，实时显示各模型厂商的额度。当前实现存在三个结构性问题，叠加 5 项用户体验增强诉求：

1. 内置 `mockData.js` 既维护**静态 provider 元数据**（catalog），又生成**随机浮动数据**（mock）；本项目已接入真实后端（MiniMax / Codex / custom extractor），随机浮动数据已无意义。
2. `mockData.js` 仍在 `providerName` 字段写死中文名（如"云之声Token Hub"），与配置页用户自定义的 `providerName` 字段语义混淆。
3. 5 项 UX 增强（手动刷新重置 / 排序 / 官网跳转 / Orb 闪烁 / Logo + 启用禁用）必须落在新的"显示模型"层，且不应该在已分裂的 mockData.js 上加补丁。

本次 change 决定：先做架构重构（mockData.js 拆分 + 真数据为唯一来源），再叠加 5 项增强。

```
原架构：
  mockData.js
    ├── 静态 catalog（SEED_PROVIDER_DEFINITIONS / providerCatalog.js）
    ├── 随机浮动（fluctuateProviders）        ← 删除
    ├── 显示模型派生（normalizeDefinitions / buildDisplayProviders）
    └── 排序 / 过滤辅助                        ← 拆出

新架构：
  providerRegistry.js
    ├── 静态 catalog（注册 defaultProviders / customProviders）
    ├── SVG logo / website / 重置周期
    └── normalize / pre-sort

  providerSnapshot.js
    ├── 抽象 refreshOne(id) → snapshot
    ├── miniMax / codex / custom 三个具体实现
    └── 错误状态

  displayProvider.js
    ├── buildDisplay(registry, snapshot) → displayProviders
    └── sortProviders / filterEnabled / firstCharFallback
```

## Goals / Non-Goals

**Goals**

- 完全删除随机浮动数据，本期所有供应商数字来源于真实 extractor / 配置文件
- 5 项 UX 增强齐全且通过 `plasmawindowed` 视觉验收
- 拆分后每个 JS 文件 < 250 行，单一职责
- 旧 Plasmoid configuration JSON 兼容（缺字段自动兜底）
- 不引入 C++ 改动、新依赖

**Non-Goals**

- 不实现 GLM / Claude / Gemini 等内置供应商的 extractor（仅静态注册 + "未配置"占位）
- 不做云端 Logo 同步、不做 ConfigModel 之外的全局快捷键
- 不做集成测试（QTest 框架未引入）
- 不修改 `docs/usage-script-spec.md` 已有的 `resetAt` 字段命名（保留向下兼容）

## Decisions

### D1. 拆分为三文件（registry / snapshot / display）

**为什么**：
- `providerRegistry.js` 静态元数据（配置 JSON 解析、catalog 合并、字段兜底）。
- `providerSnapshot.js` 实时数据（仅负责"从某供应商拉数据"，不负责"组装显示模型"）。
- `displayProvider.js` 显示模型派生（registry + snapshot → QML 友好对象）。

**备选 A**（保留 mockData.js 但删除随机浮动）：仍 1 个文件，但 500+ 行，与单一职责原则冲突。
**备选 B**（仅 1 个 providerRegistry.js 承担全部）：新装太重，且 QML 测试时无法隔离单层。

### D2. Logo 字段用内联 SVG 字符串

**为什么**：用户选择内联 SVG 字符串。`providerRegistry.js` 内 `logoSvg` 字段直接写 SVG 文本，`QML` Image.source = `"data:image/svg+xml;utf8,..."` 或赋 base64 data URL；不需要额外资源文件、能在脑外 renderer 中预览。

**字段规则**：
```js
{
  id: "minimax",
  logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>...</svg>",
  logoSize: 24
}
```

### D3. customOrder 提供 UI（在 ProvidersConfig 中实现 ▲/▼ 按钮）

**为什么**：用户选择 UI 方案。每个供应商列表项右侧两个按钮，"up" 按钮 enable 条件为 `index > 0 && sortMode === 'custom'`；"down" 按钮 enable 条件为 `index < length-1 && sortMode === 'custom'`。点击后维护 `Plasmoid.configuration.customOrder`（JSON 数组）。

**未选中 custom 模式时**：按钮隐藏。

### D4. 真实数据 → extractor 失败时显示错误状态

**为什么**：用户明确要求"不使用任何 mock"。extractor 失败 = 该供应商运行期显示 `errorText`（"凭证未配置" / "网络异常" / "认证失败"），不影响其他供应商。

**QML 端**：`ProviderGroup` 标题右侧 statusLabel 颜色随 errorText 变化；Orb 切到该供应商时仍按 tightest 已用%显示（红色 LED）。

**ProviderSnapshot 接口**：
```js
// providerSnapshot.js
.refreshOne(id, options) → {
  snapshot: { providerId, plans: [{planId, used, total, resetAt, errorText}], statusLabel, errorText },
  refreshAt: Date  // 用于 lastRefreshTime
}
```

### D5. CrossFade + 缩放替换 Orb 切换动画

**为什么**：用户选择 CrossFade。
- `providerSwitch` 替换为 `CrossFade` 容器，旧供应商 Item 与新供应商 Item 同时叠加，各自带 0.94→1 ScaleAnimator + 0.85→1 Opacity。
- `Charts.PieChart` 颜色 source 变更绑定到 `currentUsage.usageColor` 直接更新，不触发组件重建。
- 进度条 ProgressBar.value 用 `Behavior on value { NumberAnimation { duration: 150 } }` 软更新。

### D6. 已用% 排序使用 tightest 策略

**为什么**：用户选择 "已用%最大值"。`displayProvider.tightestUsage(displayProviders)` 已有等效逻辑（取每个供应商所有 plan 中已用%最大者），直接复用。

### D7. sortMode 持久化到 Plasmoid.configuration

**为什么**：用户选择持久化。`main.qml` 在 `displayProviders` 派生时读 `Plasmoid.configuration.sortMode || "default"`，变更时通过 `Plasmoid.configuration.sortMode = newMode` 写回（Plasma 自动保存到 KConfig XT）。

**customOrder** 持久化为 JSON 字符串：`Plasmoid.configuration.customOrder = JSON.stringify([id1, id2, ...])`。

### D8. 启用/禁用过滤在 displayProvider 层完成

**为什么**：用户已确认。"禁用"本质是 ordering 优先级最低（> sortMode 但 < `enabled=false`）。`displayProvider.filterEnabled(definitions)` 必须在 `sortProviders` 之前调用。

**Plasmoid.configuration 字段**：`providers` JSON 数组中每个 provider 增 `enabled`（默认 true）/`logoPath`（默认 ""）。

### D9. 官网跳转用 `Qt.openUrlExternally` + `^https?://` 校验

**为什么**：避免 `javascript:` / `file://` 注入。`ProviderGroup.qml` 内 `providerNameLabel` 在 `website` 校验通过时启用 hover cursor、`Kirigami.Link` 颜色与下划线；点击后 `Qt.openUrlExternally(website)`。

### D10. 手动刷新重置计时器用 `Timer.restart()`

**为什么**：QML `Timer` 原生 `restart()`，无需 stop+start。`main.qml` 把匿名 Timer 改为 `id: refreshTimer`；`refresh()` 末尾调用 `refreshTimer.restart()`。

## Risks / Trade-offs

- [R1] 拆分后旧路径 mockData.js 仍有引用方（`ProviderEditor.qml`）→ 一次性扫描替换；保留 1 个 shim 文件 `mockData.js` 仅导出 `forEach` 兼容入口，标记 deprecated。
- [R2] 内置 SVG 字符串在 QML `Image.source` 加载大小 > 1KB 时偶有 QImage 解析失败 → 单个 SVG 控制在 800 字以内，颜色用 `#rrggbb` 不用 `rgba()`。
- [R3] providerSnapshot 在 `displayProvider` 之前异步填充 → 界面渲染期间短暂空数据 → `ListView.opacity` 同时 0→1 渐现，避免"先空后填"的闪烁。
- [R4] customOrder 移除已删除供应商 → ProvidersConfig 中删除供应商时同步从 customOrder 剔除。
- [R5] 5 项变更同时改 `providerName` 字段语义 → 旧 mockData 的"云之声Token Hub"硬编码中文名必须替换为内置 catalog `minimax` / `codex` / `glm` / `cloud-voice` `providerName` 字段（首次 push 时由本人查询官网校正）。
- [R6] extractor 真正实现取决于 MiniMax / Codex / custom 三个接口 → 本期只实现 custom（脚本驱动）+ 临时 stub 包装 MiniMax / Codex 已有请求（不真实网络）。

## Migration Plan

1. **阶段 1**：拆分 JS 文件（`providerRegistry.js` / `providerSnapshot.js` / `displayProvider.js`），删除 `mockData.js` 内随机浮动；保留剩余 normalize / buildDisplay 函数作为初始版本。
2. **阶段 2**：在 `providerRegistry.js` 内联 6 个内置供应商的 SVG + website + providerName。
3. **阶段 3**：5 项 UX 增强逐项实现 + 单元测试验证。
4. **阶段 4**：回归测试，确保旧配置（缺 enabled / logoPath / sortMode）能正常加载。
5. **回滚**：`git revert`；无 schema 迁移。

## Open Questions

- [ ] GLM / Claude / Gemini 内置供应商的 extractor 本期是否实现 stub？（建议：仅注册静态 + 显示"未配置"占位）
- [ ] 三门峡 5 项变更后，`providerName` 字段在所有内置供应商请求中如何对齐到对应规范？在主官网抓取并校准
- [ ] `displayProvider` 函数 多 vs 10 个供应商的性能开销 → 当前规模 < 100 plan 足够
