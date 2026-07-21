# AI 用量监控 — 需求基线

> 本文档是 `aiUsageWatcher` 当前迭代的唯一产品需求基线。与旧设计、旧计划、旧代码冲突时，以本文档为准。

## 1. 目标与边界

`aiUsageWatcher` 是 KDE Plasma 6 小部件，用来查看多个 AI 服务供应商的套餐已用额度。当前迭代交付可运行的 mock 数据版本，重点验证 Plasma 小部件交互、配置管理、数据契约和 UI 一致性。

本期范围：

- compact 视图显示最紧张套餐的已用百分比，可在饼图和水平柱状图之间切换。
- full 弹出面板按供应商展示套餐，始终复用 `ProviderGroup` 和 `PlanBar`。
- 标准 Plasma applet 配置页，支持 UI 设置和供应商定义的增删改。
- mock 快照定时刷新，但运行时用量不写入 KConfig。
- Plasma 6 静态检查、JS/QML 逻辑测试和真实 `plasmawindowed` 冒烟验证。

本期不做：

- 真实 HTTP 请求、QuickJS 执行、KWallet 凭据管理。
- 脚本编辑；只显示后续版本提示。
- 历史曲线、系统监视器集成、供应商导入导出。
- 主题选择或修改系统全局主题；所有视图只跟随当前 Plasma 主题。
- full 面板的饼图/柱状图切换；`compactStyle` 只影响 compact。
- 按时间窗口分组；full 本期只按供应商分组。
- 真正的窗口管理器“总在最前”。本期只支持让 Plasma popup 在失焦时保持打开。

## 2. Plasma 6 包与技术约束

- 插件 ID、安装目录和运行命令统一为：
  - ID：`aiUsageWatcher`
  - 用户安装目录：`~/.local/share/plasma/plasmoids/aiUsageWatcher/`
  - 安装/升级：`kpackagetool6 --type Plasma/Applet --upgrade package`
  - 运行：`plasmawindowed aiUsageWatcher`
  - 卸载：`kpackagetool6 --type Plasma/Applet --remove aiUsageWatcher`
- `metadata.json` 必须包含非空 `KPlugin.Name`、`KPlugin.License`、`KPlugin.Id: "aiUsageWatcher"`、`KPackageStructure: "Plasma/Applet"` 和最低 API 版本 6.0。
- 根组件使用 `PlasmoidItem` 和标准 `compactRepresentation` / `fullRepresentation`。
- 尺寸、间距和主题使用 `Kirigami.Units`、`Kirigami.Theme`；禁止使用 `PlasmaCore.Units`、`PlasmaCore.Theme` lowercase 颜色属性和硬编码十六进制主题色。
- 控件使用 Qt Quick Controls、Plasma Components、Kirigami 和 Breeze 图标名称，不创建自定义图标资产。
- KCM 入口必须是 `package/contents/config/config.qml` 中的 `ConfigModel` / `ConfigCategory`。配置页使用 `cfg_` 属性，由 Plasma 配置对话框管理 Apply/Cancel；不得声明或依赖外部 `X-KDE-ConfigModule`。

## 3. 唯一用量语义

系统唯一百分比指标为 `usedPercent`：

- 数值范围为 `0..100`。
- 值越高表示已用越多、越紧张。
- 最紧张套餐取所有有效套餐中最大的 `usedPercent`。
- 无有效套餐返回 `-1`，UI 显示灰色和 `—`。

颜色阈值：

| `usedPercent` | 语义 | UI 颜色 |
|---|---|---|
| `< 85` | 正常 | `Kirigami.Theme.positiveTextColor` |
| `85..94` | 注意 | `Kirigami.Theme.neutralTextColor` |
| `>= 95` | 紧张 | `Kirigami.Theme.negativeTextColor` |
| 无数据 | 未知 | `Kirigami.Theme.disabledTextColor` |

`barClass` / `ledClass` 是 `usedPercent` 的派生展示字段，不是独立事实来源。边界值 84、85、94、95、100 必须有测试。

## 4. 数据契约与数据流

### 4.1 持久化的供应商定义

KConfig 的 `providers` 字段只保存供应商定义 JSON，不保存刷新后的运行时用量：

```typescript
type ProviderDefinition = {
  id: string;                 // 稳定、唯一，不以名称定位编辑项
  providerName: string;
  sourceLabel: "自定义" | "套餐" | "订阅";
  trustMode: "strict" | "lan" | "custom";
  template: string;           // provider 级模板
  plans: Array<{
    id: string;               // provider 内唯一
    planName: string;
    unit: string;
  }>;
};
```

约束：

- `id`、`providerName` 必填；provider ID 不得重复。
- 同一 provider 内 plan ID 和 planName 均不得重复。
- provider 至少有一个 plan。
- provider 模板默认 `%1 限额  %2/%3  重置于 %4`。
- 旧 `providers` JSON 若包含 `usedPercent`、`usedText` 等快照字段，规范化时只提取定义字段；无效 JSON 或错误顶层类型回退到种子定义，不能使 QML 崩溃。

### 4.2 非持久化运行时快照

mock 数据源维护独立的运行时快照：

```typescript
type RuntimeProviderSnapshot = {
  providerId: string;
  statusLabel: string;
  errorText: string;
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

由一个纯函数把定义与快照合并为 UI 只读的 `DisplayProvider[]`。每个 display plan 的以下字段必须每次从 `used`、`total` 和原始文本派生：

- `usedPercent = clamp(round(used / total * 100), 0, 100)`；`total <= 0` 或非有限数值时为 `-1`。
- `usedPercentLabel`：有效时为整数加 `%`，否则为 `—`。
- `usedText`：有效时为独立的 `used` 文本；不得预先拼接 `/ total`。
- `totalText`：有效时为独立的 `total` 文本。
- `barClass`、provider `ledClass`：由本章阈值派生。

Timer 和手动刷新只替换内存中的 runtime snapshot / display model，不写 `plasmoid.configuration.providers`。只有 KCM Apply 写 provider definitions 和 UI settings。

### 4.3 种子数据

- 云之声 Token Hub：3 个套餐；已用百分比分别约 65、22、8，均为绿色。
- MiniMax：1 个套餐，`usedPercent = 88`，黄色，带 `extraText`。
- Codex：1 个套餐，`used = 503`、`total = 750`，派生 `usedPercent = 67`，绿色。

mock 波动必须不可变更新 `used`，并在同一次派生中同步 `usedPercent`、`usedPercentLabel`、`usedText`、`totalText` 和颜色，禁止显示字段漂移。

## 5. 模板规则

模板归属 provider，所有该 provider 的 plan 共用模板。默认值：

```text
%1 限额  %2/%3  重置于 %4
```

占位符固定为：

- `%1`：`planName`
- `%2`：独立的 `usedText`
- `%3`：独立的 `totalText`
- `%4`：`resetText`

模板渲染用安全的顺序替换函数或 `i18n(template, ...)`；不得把已经含 `/ total` 的组合字符串传给 `%2`。KCM 编辑器提供实时预览，缺少 reset 时保留可读文本而不是生成 `undefined`。

## 6. UI 与交互

### 6.1 Compact

- `compactStyle` 只有 `pie`、`bar` 两个值，默认 `pie`，只影响 compact。
- compact 显示最紧张有效套餐的 `usedPercent`；无数据为 `—`。
- 左键切换 popup 展开状态；外部点击默认关闭。
- tooltip 主标题为“AI 用量监控”，副标题为最紧张 provider / plan 及已用百分比，不能只显示“点击查看详情”。
- 长 provider/plan 名称采用 elide 或 wrap，不能挤压百分比。

### 6.2 Full

- full 始终按 provider 分组，通过 `ProviderGroup` 展示 header/error，通过 `PlanBar` 展示每条套餐；不得依据 `compactStyle` 内联第二套套餐 UI。
- 标题栏为“AI 用量监控”与刷新、配置、保持打开三个 Breeze `ToolButton`。
- 刷新按钮点击后旋转 300ms 并触发一次内存刷新。
- “保持面板打开”控制根 `PlasmoidItem.hideOnWindowDeactivate = !keepPanelOpen`。依据本机 Plasma 6 官方 `org.kde.desktopcontainment/contents/ui/FolderViewLayer.qml` 的同类实现，这是 popup 保持机制，不承诺窗口管理器层面的总在最前。
- 每个图标按钮必须设置 ToolTip 和 `Accessible.name`；键盘可聚焦。
- 面板底部显示状态栏：最近刷新时间、供应商数量、有效套餐数量；无数据或错误时显示明确状态。
- 面板可滚动，窄宽度下 header 和 PlanBar 不重叠；使用 Layout、elide/wrap 和最小宽度，不使用固定减去 220px 的布局。
- `opacityPercent` 只影响 full 背景透明度，范围 20..100。

### 6.3 PlanBar

- 保留“计划名 + 弹性进度条 + 百分比”和下一行详情的既有信息层级。
- 进度条宽度动画为 300ms、`Easing.OutCubic`。
- PlanBar 接收独立字段：`planName`、`usedPercent`、`usedPercentLabel`、`usedText`、`totalText`、`unitText`、`resetText`、`extraText`、`templateText`。
- 颜色全部来自 `Kirigami.Theme`。

## 7. KCM 配置

`contents/config/config.qml` 声明两个 `ConfigCategory`：

1. 常规：`source: "config/GeneralConfig.qml"`
2. 供应商：`source: "config/ProvidersConfig.qml"`

每个页面根组件为 `KCM.SimpleKCM`，通过 `cfg_` 属性参与标准 Apply/Cancel。

### 7.1 常规页

| 用户字段 | KConfig 字段 | 控件 | 默认值 |
|---|---|---|---|
| compact 样式 | `compactStyle` | ComboBox：饼图/水平柱状图 | `pie` |
| 刷新间隔（秒） | `refreshIntervalSec` | SpinBox 10..3600 | 60 |
| 面板背景不透明度 | `opacityPercent` | Slider 20..100 | 80 |
| 保持面板打开 | `keepPanelOpen` | CheckBox | false |

不提供 `colorScheme`、`groupBy` 或“窗口置顶”字段。已有未发布配置中的这些字段可以删除；禁止修改系统全局主题。

### 7.2 供应商页

- `cfg_providers` 是待应用的 provider definitions JSON；编辑操作只修改本页工作副本和 `cfg_providers`，不得直接写 `Plasmoid.configuration`。
- 列表按 provider ID 编辑/删除，显示 provider 名称和计划摘要。
- 添加、编辑使用单个 Dialog，编辑内容组件本身不是 Dialog，禁止 Dialog 套 Dialog。
- 删除前二次确认。
- 保存按钮仅在以下条件全部满足时启用：provider 名称非空、ID 唯一、至少一个 plan、plan 名称非空且不重复、模板含 `%1`、`%2`、`%3`、`%4`。
- 使用 `list-add`、`document-edit`、`edit-delete` 等 Breeze 图标，不使用 emoji 作为操作按钮。
- 模板字段下显示基于固定示例 `5小时 / 65 / 100 / 今天 18:00` 的实时预览。
- 脚本区只读显示“脚本编辑将在后续版本实现”。

## 8. 验证和发布门槛

必须同时满足：

1. JS/QML 逻辑测试覆盖阈值、最紧张值、错误输入回退、不可变刷新、派生字段同步、KCM Apply/Cancel 工作副本语义。
2. `qmllint package/contents/ui/*.qml package/contents/ui/config/*.qml package/contents/config/config.qml` 无错误。
3. `xmllint --noout package/contents/config/main.xml` 通过。
4. 安装只使用 `kpackagetool6 --type Plasma/Applet --upgrade package`；安装后用 `diff -qr package ~/.local/share/plasma/plasmoids/aiUsageWatcher` 验证副本一致。
5. `plasmawindowed aiUsageWatcher` 日志中没有 `ReferenceError`、`TypeError`、`PlasmaCore.Units` 或组件加载失败。
6. 实际 popup 能看见三个 provider 和五条 PlanBar；不能把 `timeout` 返回 124 单独当成成功证据，必须同时审查日志并完成可见行断言/截图。
7. KCM 能打开两个分类；Apply 后保留配置，Cancel 不保存工作副本。
8. `metadata.json` 的 AppStream 元数据包含非空名称和许可证。

## 9. 验收清单

- [ ] compact 饼图和柱状图均能显示最大 `usedPercent`，无数据显示灰色 `—`。
- [ ] 阈值 `<85` 绿、`85..94` 黄、`>=95` 红；MiniMax 初始为 88 黄色。
- [ ] full 在任意 compactStyle 下都使用相同的 `ProviderGroup -> PlanBar` 树。
- [ ] popup 显示三家供应商和五条套餐，没有大面积由零高度造成的空白。
- [ ] 刷新旋转 300ms，所有派生字段同步变化，Timer 不写 KConfig。
- [ ] tooltip、三个标题栏按钮的 ToolTip/Accessible、状态栏和长文本响应式布局有效。
- [ ] “保持面板打开”只控制 popup 失焦关闭行为。
- [ ] KCM 通过标准两个 ConfigCategory 打开，常规设置遵循 Apply/Cancel。
- [ ] 供应商按 ID 增删改，校验、删除确认和模板预览有效，重启后定义保持。
- [ ] 系统亮/暗主题下颜色可读，且项目中无 `PlasmaCore.Theme` / `PlasmaCore.Units`。
- [ ] metadata、安装路径和文档统一使用 `aiUsageWatcher`。
- [ ] 全部自动检查与真实运行门槛通过。
