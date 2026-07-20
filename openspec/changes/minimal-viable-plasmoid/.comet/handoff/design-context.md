# Comet Design Handoff

- Change: minimal-viable-plasmoid
- Phase: design
- Mode: compact
- Context hash: 529976cf57ed937b4ecd7d48b61765888e47d514b1781a1e03057dd4a3a33800

Generated-by: comet-handoff.sh

OpenSpec remains the canonical capability spec. This handoff is a deterministic, source-traceable context pack, not an agent-authored summary.

## openspec/changes/minimal-viable-plasmoid/proposal.md

- Source: openspec/changes/minimal-viable-plasmoid/proposal.md
- Lines: 1-28
- SHA256: 55d2aafbe72e67924339b7c203bd4142268eefcc589dd7e99310c7eb1e9aafe8

```md
## Why

aiUsageWatcher 当前只有 UI 骨架和需求文档，无法在 Plasma 6 下实际运行。需要先建立最小可运行的版本，让 QML 组件通过 Timer + mock 数据驱动起来，验证 UI 渲染、颜色语义、交互逻辑的正确性，同时修复 `/review` 发现的代码质量问题。

## What Changes

- 将 `main.qml` 中内联的静态 mock 数据改为 Timer 驱动的动态数据源（JS 模块），模拟周期刷新
- 修复 ProviderGroup 的 `border.color` 重复赋值（当前被后续 `color` 覆盖）
- 修复错误态显示逻辑：`errorText` 可见性条件补充空数组检查
- 修复 `providerName` 后缀剥离：自动去掉 ` · Claude` / ` · Codex` 等后缀
- 修复 `tightestUsedPercent()` / `tightestProviderName()` 中无 plan 时的行为
- 移除 `configGeneral.qml` 空壳中的无效 import
- 确保 `qmllint` 在所有 .qml 文件上无错误
- 补充 `README.md` 开发运行说明

## Capabilities

### New Capabilities
- `mock-data-timer`: Timer 驱动的 mock 数据源，模拟供应商/套餐用量定期刷新，支持 QML 组件绑定刷新

### Modified Capabilities

（无）

## Impact

- **修改文件**：`package/contents/ui/main.qml`、`ProviderGroup.qml`、`PlanBar.qml`、`Orb.qml`、`configGeneral.qml`、`README.md`
- **新增文件**：`package/contents/js/` 目录（mock 数据源 JS 模块）
- **不涉及**：C++ backend、KConfig、KWallet、NetworkManager、QuickJS、KCM```

## openspec/changes/minimal-viable-plasmoid/design.md

- Source: openspec/changes/minimal-viable-plasmoid/design.md
- Lines: 1-86
- SHA256: a57bc3147ac21ad5c9dfcc1bf54b83c9474dce1c31ed60934f21fe3fc135005d

[TRUNCATED]

```md
## Context

aiUsageWatcher 是 KDE Plasma 6 桌面小部件，用于实时监控各大模型厂家的模型套餐用量。当前只有 UI 骨架（main.qml / ProviderGroup / PlanBar / Orb），使用内联静态 mock 数据。需要让 Timer 驱动数据刷新，验证 UI 动态更新正确性，同时修复代码质量问题。

**技术约束：**
- 纯 QML 实现，无 C++ backend
- Plasma 6 API：`PlasmoidItem` + `compactRepresentation`/`fullRepresentation`
- 开发运行：`plasmawindowed aiUsageWatcher`
- 静态检查：`qmllint`

## Goals / Non-Goals

**Goals:**
- Timer 驱动的 mock 数据源，模拟 60s 刷新周期
- UI 组件正确绑定刷新，颜色语义随 usedPercent 阈值切换
- 修复 ProviderGroup 的 `border.color` 重复赋值
- 修复错误态显示逻辑
- 实现 providerName 后缀剥离
- 修复无 plan 时的边缘情况
- `qmllint` 无错误

**Non-Goals:**
- KWallet 凭据存储
- 真实 HTTP 请求
- QuickJS 沙箱
- KCM 配置界面
- 多供应商管理

## Decisions

### D1: Mock 数据源位置与格式

**决定：** 在 `package/contents/js/mockData.js` 创建 JS 模块，导出 `MockProviders` 数组。

**备选方案：**
| 方案 | 优点 | 缺点 |
|---|---|---|
| 内联 Timer + 数组 | 最简单 | 数据与逻辑耦合 |
| 外部 JS 模块 | 分离关注点，易替换 | 需要额外 import |
| QML DataModel | Qt 原生 | 过重，后续要改 |

**理由：** JS 模块便于后续替换为真实数据源，且 QML 支持直接 `import "js/mockData.js" as MockData`。

### D2: Timer 刷新周期

**决定：** 使用 60s 间隔，与 requirements.md 默认刷新间隔一致。

**代码位置：** `main.qml` 内的 `Timer { interval: 60000; running: true; repeat: true }`

### D3: 错误态可见性修复

**决定：** 将 `visible: groupRoot.errorText.length > 0 && groupRoot.plans.length === 0` 改为 `visible: groupRoot.errorText.length > 0`（ProviderGroup 的 plans 为空数组时仍显示错误）。

**理由：** 原条件 `plans.length === 0` 会隐藏有错误文本但无 plan 的供应商，但错误态就是要显示错误信息。

### D4: ProviderGroup 边框颜色

**决定：** 删除第一个 `border.color: Qt.rgba(1, 1, 1, 0.08)` 赋值，只保留 switch 表达式的赋值。

### D5: providerName 后缀剥离

**决定：** 在 `main.qml` 的 `providers` 数据源中剥离，不修改 UI 层。

**函数：**
```js
function stripProviderSuffix(name) {
    return name.replace(/\s*·\s*(Claude|Codex|OpenCode|Cursor|Windsurf)$/i, '').trim();
}
```

### D6: 无 plan 时的边缘情况

**决定：** 
- `tightestUsedPercent()` 返回 `-1` 表示无数据，Orb 显示灰色 `"—"`
- `tightestProviderName()` 返回空字符串

## Risks / Trade-offs

| 风险 | 缓解措施 |
|---|---|
```

Full source: openspec/changes/minimal-viable-plasmoid/design.md

## openspec/changes/minimal-viable-plasmoid/tasks.md

- Source: openspec/changes/minimal-viable-plasmoid/tasks.md
- Lines: 1-31
- SHA256: cd18c6b146d3814257f249434df0ec1a93f990e6f458f1781494b9b180a2a7a0

```md
## 1. Mock Data Source

- [ ] 1.1 Create `package/contents/js/mockData.js` with `MockProviders` array matching the current `providers` structure
- [ ] 1.2 Add `stripProviderSuffix(name)` function to mockData.js
- [ ] 1.3 Add `fluctuateProviders(providers)` function to randomly adjust usedPercent ±5%

## 2. Timer Integration

- [ ] 2.1 Add `import "js/mockData.js" as MockData` to main.qml
- [ ] 2.2 Create `Timer` component with `interval: 60000`, `running: true`, `repeat: true`
- [ ] 2.3 Add `onTriggered` handler to update `providers` with fluctuated data

## 3. Bug Fixes

- [ ] 3.1 Fix ProviderGroup.qml: remove duplicate `border.color` assignment (keep only the switch expression)
- [ ] 3.2 Fix ProviderGroup.qml: change error label visibility from `errorText.length > 0 && plans.length === 0` to `errorText.length > 0`
- [ ] 3.3 Fix main.qml: apply `stripProviderSuffix()` to provider names in tightestProviderName()
- [ ] 3.4 Fix Orb.qml: handle `tightestUsedPercent() < 0` with gray ring and "—" label

## 4. Cleanup

- [ ] 4.1 Remove unused import from configGeneral.qml
- [ ] 4.2 Run `qmllint` on all QML files and fix any errors
- [ ] 4.3 Update README.md with `plasmawindowed aiUsageWatcher` development instructions

## 5. Verification

- [ ] 5.1 Run `plasmawindowed aiUsageWatcher` and verify compact orb displays
- [ ] 5.2 Click orb to expand full view and verify all providers render
- [ ] 5.3 Wait 60s and verify timer triggers data refresh with fluctuated values
- [ ] 5.4 Verify color transitions: red (≤5%), yellow (≤15%), green (>15%)
- [ ] 5.5 Verify error state displays when provider has errorText and empty plans```

## openspec/changes/minimal-viable-plasmoid/specs/mock-data-timer/spec.md

- Source: openspec/changes/minimal-viable-plasmoid/specs/mock-data-timer/spec.md
- Lines: 1-94
- SHA256: e97305fa85529db360d8b1fe3640b89b04ccdd775ad1ee308f805784c93b73b3

[TRUNCATED]

```md
## ADDED Requirements

### Requirement: Mock data timer refresh

The system SHALL use a QML `Timer` component to periodically refresh mock provider data at 60-second intervals.

#### Scenario: Timer triggers data refresh
- **WHEN** the timer fires at 60-second interval
- **THEN** the `providers` property on `PlasmoidItem` SHALL be updated with new mock data
- **THEN** all UI components bound to `providers` SHALL re-render

#### Scenario: Timer starts on plasmoid load
- **WHEN** the plasmoid is loaded
- **THEN** the timer SHALL start running immediately (`running: true`)
- **THEN** the timer SHALL repeat indefinitely (`repeat: true`)

### Requirement: Mock data seed structure

Mock data SHALL start with three seed providers covering different UI states:

1. **云之声Token Hub** — multi-plan (5小时/7天/30天), all green range
2. **MiniMax** — single plan (余额), yellow range (88%), with extraText
3. **Codex** — single plan (周限额), green range (~67%), weekly reset

#### Scenario: Seed provider 1 — 云之声Token Hub
- **WHEN** mock data is initialized
- **THEN** provider SHALL have `providerName: "云之声Token Hub"`, `ledClass: "led-green"`, `sourceLabel: "自定义"`, `statusLabel: "可用"`
- **THEN** SHALL have 3 plans with varying usedPercent (all green)
- **THEN** plans SHALL have different resetText: "今天 18:00", "周日 00:00", "2026-08-20重置"

#### Scenario: Seed provider 2 — MiniMax
- **WHEN** mock data is initialized
- **THEN** provider SHALL have `providerName: "MiniMax · Claude"`, `ledClass: "led-yellow"`, `sourceLabel: "套餐"`, `statusLabel: "降级"`
- **THEN** SHALL have 1 plan with `usedPercent: 88`, `barClass: "bar-yellow"`, `extraText: "活动期 8 月底结束"`

#### Scenario: Seed provider 3 — Codex
- **WHEN** mock data is initialized
- **THEN** provider SHALL have `providerName: "Codex"`, `ledClass: "led-green"`, `sourceLabel: "订阅"`, `statusLabel: "可用"`
- **THEN** SHALL have 1 plan with `planName: "周限额"`, `usedPercent: 67`, `barClass: "bar-green"`
- **THEN** plan SHALL have `resetText: "周日 00:00"`, `usedText: "503 / 750 次"`, `unitText: "次"`, `extraText: "Codex CLI 请求限额"`

### Requirement: Provider name suffix stripping

The system SHALL strip known AI assistant suffixes from provider names (e.g., ` · Claude`, ` · Codex`, ` · OpenCode`, ` · Cursor`, ` · Windsurf`).

#### Scenario: Suffix stripped from provider name
- **WHEN** a provider name ends with ` · Claude` or ` · Codex`
- **THEN** the suffix SHALL be removed for display
- **THEN** the original name SHALL be preserved in the data source

### Requirement: Error state visibility

A ProviderGroup with `errorText` SHALL display the error message even when `plans` array is empty.

#### Scenario: Error shown without plans
- **WHEN** a provider has `errorText: "something went wrong"` and `plans: []`
- **THEN** the error label SHALL be visible
- **THEN** the error label SHALL show the error text

#### Scenario: Error hidden when plans exist
- **WHEN** a provider has both `errorText` and `plans` with entries
- **THEN** the error label SHALL be hidden
- **THEN** the plans SHALL render normally

### Requirement: Empty/no-data state

When no provider has any plans, the compact orb SHALL display a gray ring with `"—"`.

#### Scenario: No plans across all providers
- **WHEN** all providers have `plans: []` or no providers exist
- **THEN** `tightestUsedPercent()` SHALL return -1
- **THEN** orb SHALL show gray ring with `"—"`
- **THEN** `tightestProviderName()` SHALL return empty string

### Requirement: QML static analysis

All `.qml` files SHALL pass `qmllint` without errors.

#### Scenario: qmllint passes on all QML files
- **WHEN** `qmllint` is run on each `.qml` file in `package/contents/ui/`
```

Full source: openspec/changes/minimal-viable-plasmoid/specs/mock-data-timer/spec.md

