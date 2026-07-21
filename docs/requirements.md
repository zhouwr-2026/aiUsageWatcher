# AI 用量监控 — 需求基线

> 本文档是 `aiUsageWatcher` 当前迭代的唯一产品需求基线。与旧设计、旧计划、旧代码冲突时，以本文档为准。

## 1. 目标与边界

`aiUsageWatcher` 是 KDE Plasma 6 小部件，用来查看多个 AI 服务供应商的套餐已用额度。当前迭代在既有 UI/KCM 闭环上加入原生查询后端，先接通固定 MiniMax Token Plan，并保留其他供应商的模拟数据作为后续接入占位。

本期范围：

- compact 视图按配置顺序轮巡供应商，显示当前供应商最紧张套餐的已用百分比，可在饼图和水平柱状图之间切换。
- compact 悬停内容必须对应当前轮巡到的供应商/套餐；点击展开 full 悬浮面板。
- full 弹出面板按供应商展示全部套餐，包括 5 小时、周、月等已返回额度，始终复用 `ProviderGroup` 和 `PlanBar`。
- 标准 Plasma applet 配置页，支持 UI 设置和供应商定义的增删改。
- C++ 后端查询 MiniMax 官方 Token Plan remains 接口；其他模拟快照仍可定时刷新，运行时用量不写入 KConfig。
- Plasma 6 静态检查、JS/QML 逻辑测试和真实 `plasmawindowed` 冒烟验证。

本期不做：

- QuickJS 执行和自定义脚本真实请求。
- MiniMax API Key 配置表单与 KWallet 保存；当前开发版仅从 `MINIMAX_API_KEY` 进程环境读取，禁止写入 KConfig、源码或日志。
- Codex 浏览器登录与真实套餐查询；作为 MiniMax 后的下一项能力实现。
- 基于 Agent/模型调用事件即时切换 compact；本期先实现固定间隔轮巡，事件模式见 6.3。
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
  - QML 包目录：`~/.local/share/plasma/plasmoids/aiUsageWatcher/`
  - C++ Applet 插件：`lib/qt6/plugins/plasma/applets/aiUsageWatcher.so`
  - 构建/安装：CMake 同时安装 QML 包和 C++ 插件；单独使用 `kpackagetool6` 不能完成原生后端安装。
  - 运行：`plasmawindowed aiUsageWatcher`
  - 卸载：`kpackagetool6 --type Plasma/Applet --remove aiUsageWatcher`
- `metadata.json` 必须包含非空 `KPlugin.Name`、`KPlugin.License`、`KPlugin.Id: "aiUsageWatcher"`、`KPackageStructure: "Plasma/Applet"` 和最低 API 版本 6.0。
- 根组件使用 `PlasmoidItem` 和标准 `compactRepresentation` / `fullRepresentation`。
- 尺寸、间距和主题使用 `Kirigami.Units`、`Kirigami.Theme`；禁止使用 `PlasmaCore.Units`、`PlasmaCore.Theme` lowercase 颜色属性和硬编码十六进制主题色。
- 控件使用 Qt Quick Controls、Plasma Components、Kirigami 和 Breeze 图标名称，不创建自定义图标资产。
- KCM 入口必须是 `package/contents/config/config.qml` 中的 `ConfigModel` / `ConfigCategory`。配置页使用 `cfg_` 属性，由 Plasma 配置对话框管理 Apply/Cancel；不得声明或依赖外部 `X-KDE-ConfigModule`。
- 网络请求和凭据访问必须位于 C++ 后端。QML 只接收去敏后的 `RuntimeProviderSnapshot`，不得持有 Bearer Token。

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

模拟数据源和原生供应商后端都输出同一种独立运行时快照：

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

Timer 和手动刷新只替换内存中的 runtime snapshot / display model，不写 `plasmoid.configuration.providers`。只有 KCM Apply 写 provider definitions 和 UI settings。真实供应商快照不得再被 mock 波动函数修改。

### 4.3 种子数据

- 云之声 Token Hub：3 个套餐；已用百分比分别约 65、22、8，均为绿色。
- MiniMax：无假用量。未加载后端或未配置临时密钥时显示明确灰色状态；真实响应可产生当前周期与周额度等多条计划。
- Codex：1 个套餐，`used = 503`、`total = 750`，派生 `usedPercent = 67`，绿色。

mock 波动必须不可变更新 `used`，并在同一次派生中同步 `usedPercent`、`usedPercentLabel`、`usedText`、`totalText` 和颜色，禁止显示字段漂移。MiniMax 响应中的 `remaining_percent` 必须转换为 `used = 100 - remaining`；状态 3（未订阅）不得生成绿色假进度条。

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
- compact 按 provider definitions/display providers 的顺序每 5 秒轮巡一次；配置变化时从第一项重新开始。
- 每次只显示当前供应商最紧张有效套餐的 `usedPercent`；无数据为 `—`，但仍保留当前供应商名称供 tooltip 说明。
- 左键切换 popup 展开状态；外部点击默认关闭。
- 右键菜单复用 Plasma 自动提供的标准“配置…”入口；自定义 contextual action 只增加“刷新”，不得重复添加配置项。
- tooltip 主标题为“AI 用量监控”，副标题必须与 compact 当前轮巡项一致，显示 provider / plan / 已用百分比；当前供应商无数据时显示该供应商“暂无可用数据”，不能只显示“点击查看详情”。
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

### 6.3 Compact 调度模式

当前只实现 `polling` 轮巡模式：

- 轮巡对象是已配置供应商，不是所有套餐逐条轮巡；一个供应商有多条额度时，compact 选择其中最大 `usedPercent`。
- full 始终显示所有供应商的全部有效额度，不受 compact 当前轮巡项影响。
- 供应商无数据或请求失败时仍参与轮巡，以灰色 `—` 和明确 tooltip 暴露问题，不能静默隐藏。

后续增加 `event` 事件模式：

- Agent、CLI 或模型调用器通过本地 D-Bus 活跃事件显式上报 `providerId`，不得通过扫描任务正文、凭据文件或全局进程命令行推断。
- 单个供应商活跃时立即切换 compact；多个供应商并发活跃时只在活跃集合内轮巡。
- 活跃事件带过期时间并做去抖；事件全部过期后自动回到普通轮巡。
- 没有安装事件发送端时必须自然降级为普通轮巡，不能影响额度刷新。

### 6.4 PlanBar

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
4. CMake 构建并安装 QML 包和原生 Applet 插件；安装后验证 QML 副本一致、`aiUsageWatcher.so` 存在且运行日志出现原生后端加载证据。
5. `plasmawindowed aiUsageWatcher` 日志中没有 `ReferenceError`、`TypeError`、`PlasmaCore.Units` 或组件加载失败。
6. 使用给定 MiniMax 成功样本时，实际 popup 能看见三个 provider 和六条 PlanBar（MiniMax 当前周期与周额度各一条）；不能把 `timeout` 返回 124 单独当成成功证据，必须同时审查日志并完成可见行断言/截图。
7. KCM 能打开两个分类；Apply 后保留配置，Cancel 不保存工作副本。
8. `metadata.json` 的 AppStream 元数据包含非空名称和许可证。

## 9. 验收清单

- [ ] compact 饼图和柱状图按供应商轮巡，显示当前供应商最大 `usedPercent`；无数据显示灰色 `—`。
- [ ] compact 点击展开 full；tooltip 与当前轮巡 provider/plan/percent 一致。
- [ ] 阈值 `<85` 绿、`85..94` 黄、`>=95` 红；颜色使用已用百分比而不是剩余百分比。
- [ ] full 在任意 compactStyle 下都使用相同的 `ProviderGroup -> PlanBar` 树。
- [ ] popup 显示所有已配置供应商和其全部 5 小时/周/月等额度，没有大面积由零高度造成的空白。
- [ ] MiniMax 实时 JSON 被动态解析，未订阅模型不显示假额度，错误/无密钥状态明确可见。
- [ ] 刷新旋转 300ms，所有派生字段同步变化，Timer 不写 KConfig。
- [ ] tooltip、三个标题栏按钮的 ToolTip/Accessible、状态栏和长文本响应式布局有效。
- [ ] “保持面板打开”只控制 popup 失焦关闭行为。
- [ ] KCM 通过标准两个 ConfigCategory 打开，常规设置遵循 Apply/Cancel。
- [ ] 供应商按 ID 增删改，校验、删除确认和模板预览有效，重启后定义保持。
- [ ] 系统亮/暗主题下颜色可读，且项目中无 `PlasmaCore.Theme` / `PlasmaCore.Units`。
- [ ] metadata、安装路径和文档统一使用 `aiUsageWatcher`。
- [ ] 全部自动检查与真实运行门槛通过。
