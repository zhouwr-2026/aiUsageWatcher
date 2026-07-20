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
- **不涉及**：C++ backend、KConfig、KWallet、NetworkManager、QuickJS、KCM