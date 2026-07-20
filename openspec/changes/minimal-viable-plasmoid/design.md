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
| `plasmawindowed` 在 Wayland 下行为异常 | 先在 X11 环境验证，记录 Wayland 问题 |
| Timer 与 UI 刷新不同步 | 使用 `onTriggered` 时直接赋值 `providers` 属性，触发 QML 绑定更新 |
| mock 数据结构与未来真实数据不匹配 | 按 `docs/usage-script-spec.md` 的 extractor 返回格式设计 mock 数据结构 |

## Open Questions

（无）