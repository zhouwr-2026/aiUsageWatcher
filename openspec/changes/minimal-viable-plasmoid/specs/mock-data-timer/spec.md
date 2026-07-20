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
- **THEN** no errors SHALL be reported

### Requirement: ProviderGroup border consistency

ProviderGroup SHALL use a single `border.color` assignment determined by the `ledClass` property.

#### Scenario: Border color follows ledClass
- **WHEN** `ledClass` is `"led-green"`
- **THEN** border SHALL be `rgba(0.2, 0.82, 0.6, 0.2)`
- **WHEN** `ledClass` is `"led-yellow"`
- **THEN** border SHALL be `rgba(0.98, 0.75, 0.14, 0.2)`
- **WHEN** `ledClass` is `"led-red"`
- **THEN** border SHALL be `rgba(0.97, 0.44, 0.44, 0.3)`
- **WHEN** `ledClass` is `"led-gray"`
- **THEN** border SHALL be `rgba(1, 1, 1, 0.08)`