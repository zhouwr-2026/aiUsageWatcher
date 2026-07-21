---
comet_change: minimal-viable-plasmoid
role: technical-design
canonical_spec: openspec
base_ref: ebbf3df2892413a10c64a2637d812dd2e961171e
---

# aiUsageWatcher 可运行闭环设计

## 目标

把当前能启动但核心列表和 KCM 不可用的小部件收敛为一个可测试闭环：统一已用百分比语义，修复 Plasma 6 运行错误，full 复用既有组件，配置与运行快照分离，并通过标准 applet KCM 完成设置和供应商定义 CRUD。

唯一需求基线是 `docs/requirements.md`，脚本边界是 `docs/usage-script-spec.md`。

## 已选方案

采用“单一 display model + 两种外壳”的方案：

- `mockData.js` 负责定义/快照规范化、派生和刷新纯函数。
- `main.qml` 持有 provider definitions、runtime snapshots、DisplayProvider[] 三类状态；Timer 只更新后两者。
- compact 从 display model 取最大 `usedPercent`，`compactStyle` 只切换 compact 的 pie/bar 外观。
- full 永远是 `FullView -> ProviderGroup -> PlanBar`，删除内联 plan pie/bar 分支。
- KCM 只修改 definitions JSON 和四个 UI 设置，通过 `cfg_` 交给 Apply/Cancel。

未选方案：保留 full 双 UI 会继续产生重复逻辑；直接在 KCM 写 `Plasmoid.configuration` 会破坏 Cancel；把刷新快照写 KConfig 会造成字段漂移和每分钟写配置。

## 数据模型

### ProviderDefinition（KConfig）

```js
{
  id: "minimax",
  providerName: "MiniMax",
  sourceLabel: "套餐",
  trustMode: "strict",
  template: "%1 限额  %2/%3  重置于 %4",
  plans: [{ id: "balance", planName: "余额", unit: "$" }]
}
```

### RuntimeProviderSnapshot（仅内存）

```js
{
  providerId: "minimax",
  statusLabel: "降级",
  errorText: "",
  plans: [{
    planId: "balance", planName: "余额", used: 88, total: 100,
    unit: "$", resetText: "", extraText: "活动期 8 月底结束",
    isValid: true, invalidReason: ""
  }]
}
```

`buildDisplayProviders(definitions, snapshots)` 每次派生 `usedPercent`、两个数值文本和颜色。唯一阈值为 `<85 green`、`85..94 yellow`、`>=95 red`；无效/无数据灰色；compact 取最大值。

旧 providers JSON 只提取 definition 字段；错误 JSON、null、object、缺 plans 均安全回退。新建但无快照的 provider 显示灰色“暂无用量”，不伪造数据。

## 组件职责

- `main.qml`：配置读取、三层状态、刷新、tooltip、popup 保持状态和 representations 接线。
- `mockData.js`：`normalizeDefinitions(raw)`、`createSeedSnapshots(definitions)`、`fluctuateSnapshots(snapshots, randomFn)`、`buildDisplayProviders(definitions, snapshots)`、`tightestUsage(providers)`。
- `CompactView.qml`：只渲染 tightest usage 的 pie/bar，使用 `Kirigami.Units/Theme`。
- `FullView.qml`：标题栏、ListView、状态栏；delegate 只实例化 ProviderGroup。
- `ProviderGroup.qml`：provider header/error 与 PlanBar repeater。
- `PlanBar.qml`：既有两行信息层级、模板和响应式进度条。

FullView 不接收 `compactStyle`，从接口上阻止再次分叉。

## KCM

`contents/config/config.qml` 使用 `org.kde.plasma.configuration`：

```qml
ConfigModel {
    ConfigCategory { name: i18n("常规"); icon: "configure"; source: "config/GeneralConfig.qml" }
    ConfigCategory { name: i18n("供应商"); icon: "network-server"; source: "config/ProvidersConfig.qml" }
}
```

每个 source 页是 `KCM.SimpleKCM`：

- General 暴露 `cfg_compactStyle`、`cfg_refreshIntervalSec`、`cfg_opacityPercent`、`cfg_keepPanelOpen`。
- Providers 暴露 `property string cfg_providers`，内部 ListModel 是工作副本，每次编辑后序列化回该属性，但不直接写配置对象。
- ProviderEditor 是普通 Item/FormLayout，被唯一 Dialog 承载；按稳定 ID 编辑。
- 保存前校验名称、ID、至少一个 plan、plan 名称/ID唯一和模板四个占位符。

项目不声明外部 KCM，不实现 `colorScheme` / `groupBy`，不修改系统主题。

## Popup 保持机制

本机 Plasma 6 官方 Folder View 使用：

```qml
onCheckedChanged: root.hideOnWindowDeactivate = !checked
```

因此本项目把用户文案命名为“保持面板打开”，实现为根 `PlasmoidItem.hideOnWindowDeactivate = !keepPanelOpen`。它不是窗口管理器 always-on-top。默认 false，保留 Plasma 外部点击关闭的标准行为。

## UI 规则

- 所有颜色和间距来自 `Kirigami.Theme/Units`。
- warning 用 `neutralTextColor`，无数据用 `disabledTextColor`。
- 三个 ToolButton 有 Breeze icon、ToolTip、Accessible.name；刷新 icon 旋转 300ms。
- full 背景不透明度来自 20..100 的 `opacityPercent`。
- provider 名、plan 名和详情使用 Layout + elide/wrap；不使用 `parent.width - 220`。
- 状态栏显示最近刷新时间、供应商数、有效套餐数或错误/空状态。

## 错误处理

- 配置 parse/shape 失败：warning 一次并使用 seed definitions。
- runtime 某 provider 错误：显示 provider error，其他 provider 正常渲染。
- used/total 非有限或 total <= 0：plan 灰色、`usedPercent=-1`，不参与 tightest。
- KCM 无效输入：就地错误文本，保存禁用；Cancel 丢弃工作副本。

## 测试策略

1. `qmltestrunner`：阈值边界、最大值、规范化回退、不可变波动、派生同步、FullView PlanBar 数量、KCM 工作副本。
2. `qmllint`、`xmllint`、metadata/AppStream 检查。
3. 用 `kpackagetool6 --upgrade package` 安装，并 `diff -qr` 确保运行副本一致。
4. `plasmawindowed` 日志必须无 ReferenceError/TypeError/加载错误；同时验证 3 provider/5 PlanBar 实际可见。超时 124 只能说明进程仍运行，不能替代日志和 UI 断言。

## 交付顺序

先建立失败测试和数据契约，再修 runtime shell/compact、full 组件复用、KCM General、KCM Providers，最后做安装级冒烟与文档收口。每一阶段可独立审查并提交。
