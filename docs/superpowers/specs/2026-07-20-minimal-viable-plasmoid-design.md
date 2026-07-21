---
comet_change: minimal-viable-plasmoid
role: technical-design
canonical_spec: openspec
---

# AI 用量监控 — 最小可行小部件设计文档

## 上下文

aiUsageWatcher 是 KDE Plasma 6 桌面小部件，监控各大 AI 模型供应商的套餐用量。当前项目只有 UI 骨架（QML 组件）和需求文档，无法实际运行。需要先建立最小可运行版本，验证 UI 渲染、颜色语义、交互逻辑，同时修复代码质量问题。

## 技术方案

### 数据架构

```
┌─────────────────────────────────────────────────────────┐
│                    main.qml (PlasmoidItem)               │
│                                                         │
│  imports: "js/mockData.js" as MockData                  │
│                                                         │
│  Timer { interval: 60000; running: true; repeat: true } │
│  onTriggered → providers = MockData.fluctuate(seed)     │
│                                                         │
│  providers: [Provider] → compact/full 绑定              │
│                                                         │
│  groupBy: "provider" | "window"  (切换状态)              │
│    → 控制渲染排列方式                                    │
└─────────────────────────────────────────────────────────┘

┌─ package/contents/js/mockData.js ───────────────────────┐
│                                                         │
│  SEED_PROVIDERS: Provider[]  (3 个种子供应商)            │
│  stripProviderSuffix(name): string                      │
│  fluctuateProviders(providers): Provider[]              │
│    → 每个 plan 的 usedPercent ± random(0-5)             │
│    → 重算 usedPercentLabel, barClass                    │
│    → 不可变更新（新数组触发 QML 绑定）                    │
└─────────────────────────────────────────────────────────┘
```

### 种子供应商

| 供应商          | 维度                        | 状态 | 颜色 | 特点                   |
| --------------- | --------------------------- | ---- | ---- | ---------------------- |
| 云之声Token Hub | 5h(65%) / 7d(22%) / 30d(8%) | 可用 | 🟢   | 多 PlanBar、多重置时间 |
| MiniMax         | 余额(12%)                   | 降级 | 🟡   | 黄色阈值、extraText    |
| Codex           | 周限额(67%) 503/750次       | 可用 | 🟢   | 次/周单位、usedText    |

### 未来内置供应商

| 供应商                             | 类型           | 维度                                           |
| ---------------------------------- | -------------- | ---------------------------------------------- |
| **余额型**                         |                |                                                |
| GLM                                | 套餐型         | 5小时窗口 + 7天周限额                          |
| StepFun / SiliconFlow / OpenRouter | 余额型         | 余额                                           |
| **套餐型**                         |                |                                                |
| Kimi                               | 套餐型         | 5小时滚动窗口 + 7天周额度                      |
| MiniMax                            | 套餐型         | 5小时窗口 + 7天窗口                            |
| ZenMux                             | 套餐型         | 5小时窗口 + 7天窗口 + 月限额                   |
| 火山方舟                           | 套餐型         | 5小时窗口 + 周限额(周一重置) + 月限额(1日重置) |
| **订阅型**                         |                |                                                |
| Claude / Gemini                    | 订阅型         | 月限额                                         |
| Codex                              | 订阅型         | 周限额                                         |
| OpenCode Go                        | 订阅型         | 5小时窗口 + 7天窗口 + 月限额                   |
| GitHub Copilot                     | 订阅型（可选） | 月限额                                         |

### 展示页设计

**标准 Plasma 6 小部件布局：** compactRepresentation + fullRepresentation（悬浮面板）

**compact 视图（桌面图标）：**

- 圆球显示最紧张供应商的已用百分比
- 圆球颜色随阈值切换（≤5% 红、≤15% 黄、>15% 绿）
- 圆球下方不显示任何文字标签
- 鼠标悬停显示 tooltip：各供应商用量摘要
- 点击圆球弹出悬浮面板（fullRepresentation），非弹窗对话框

**full 视图（悬浮面板）：**

- 紧贴小部件的悬浮浮层，非独立弹窗
- 半透明深色背景，圆角
- 标题栏：AI 用量监控 + 配置按钮（图标） + 固定按钮（图标）
- 供应商列表：可滚动，每个供应商一行标题 + 多条 PlanBar
- 底部状态栏：供应商总数

### 图表与图标样式规范（主题跟随）

**原则**：所有用量可视化必须使用 KDE 6 官方/主题化的图表组件，不得自行绘制 Rectangle 模拟进度条，也不得硬编码颜色。

**组件选型（实测修正）**：

1. ~~**`org.kde.quickcharts`**~~（**实测不可用 — 见下方说明**）
2. **自研纯 QML 组件 `package/contents/ui/PieChart.qml` + `LineChart.qml`**:
   - 基于 `Canvas` 绘制,内部取色来自 `PlasmaCore.Theme`,自动跟随亮/暗主题
   - 性能:每次 `data`/`values` 变化触发 `requestPaint()`,重绘开销可控(单组件 < 1ms)
3. **`PlasmaCore.IconItem` / `PlasmaCore.SvgItem`** — 用于所有图标按钮（配置、刷新、固定）
   - 通过 `PlasmaCore.Theme` 取色,自动适配亮/暗主题

**`org.kde.quickcharts` 不可用说明（2026-07-21 实测）**：

- 物理文件 `PieChartControl.qml` / `LineChartControl.qml` 都在 `/usr/lib/qt6/qml/org/kde/quickcharts/controls/` 下
- 但 `qmldir` 引用 `plugin QuickChartsControlsplugin`,实际磁盘只有 `libQuickChartsControlsplugin.so`(Manjaro 当前包命名 bug,QML loader 找不到)
- 结果:`import org.kde.quickcharts.controls` 后,QML runtime 报 `LineChartControl is not a type`,部分场景还触发 SIGSEGV 段错误
- 结论:**不依赖 quickcharts,改用自研 Canvas 组件**

**颜色取色规则**：

- 禁止硬编码 `#34d399` / `#fbbf24` / `#f87171` 等十六进制颜色
- 改用 `PlasmaCore.Theme.HighlightColor`(主色) / `PlasmaCore.Theme.PositiveText`(绿) / `PlasmaCore.Theme.NegativeText`(红) / `PlasmaCore.Theme.NeutralText`(灰)
- 阈值切换逻辑保留(`≤5%` / `≤15%` / `>15%`),但颜色值改为从 Theme 派生

**图标库**：

- 必须使用 Breeze 图标主题中真实存在的图标(已查 `/usr/share/icons/breeze/` 验证)
- 每个图标引用必须在文档中标注真实路径,例如 `utilities-system-monitor` 对应 `apps/48/utilities-system-monitor.svg`

**compact 圆球目标效果**：

- 内嵌 `PieChart`(自研 Canvas 组件),圆环/饼图显示当前最紧张供应商用量占比
- 圆球边框颜色随阈值切换,但取自 `PlasmaCore.Theme`
- 中心文字显示整数百分比,字号与字号粗细跟随 Plasma 主题

**full 面板 PlanBar 目标效果**：

- 每条 PlanBar 始终使用 `Rectangle` 宽度动画(300ms `Easing.OutCubic`)，**页面样式不受 displayStyle 切换影响**（用户 2026-07-21 明确要求:不改 PlanBar 区域布局）
- 颜色取自 `PlasmaCore.Theme`

**分组切换（按供应商 / 按时间窗口）：**

- 默认按供应商分组
- 可切换为按时间窗口分组
- 时间窗口兼容任意长度（3小时/4小时/5小时/7天/周/月等）

**渲染规则：**

- 按供应商分组：每个 provider 一段，内部堆叠 PlanBar
- 按时间窗口分组：按 `plan.windowLabel` 聚合，每个窗口一段，内部列各供应商的 PlanBar
- 窗口标签兼容任意长度
- 默认分组存储在 KConfig，用户选择后持久化

### 配置页设计

**KCM 标准布局，底部按钮符合 KDE 规范：**

```
┌─────────────────────────────────────────────────────────────────┐
│  AI 用量监控 — 设置                                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌── 供应商列表 ────────────────────────────────────────────┐   │
│  │                                                  [+ 添加] │   │
│  │  ┌─────────────────────────────────────────────────────┐ │   │
│  │  │ ● 云之声Token Hub  3个限额   ████████░░ 65%  [✏️][🗑]│ │   │
│  │  │ ● MiniMax          1个限额   █████████░ 12%  [✏️][🗑]│ │   │
│  │  │ ● Codex            1个限额   █████░░░░░ 67%  [✏️][🗑]│ │   │
│  │  └─────────────────────────────────────────────────────┘ │   │
│  └───────────────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌── 常规设置 ───────────────────────────────────────────────┐   │
│  │  刷新间隔: [60] 秒                                         │   │
│  │  背景透明度: [████████░░░░] 80%                            │   │
│  │  窗口置顶: [✓]                                             │   │
│  │  默认分组: [▼ 按供应商]                                    │   │
│  └───────────────────────────────────────────────────────────┘   │
│                                                                  │
│  [默认值]  [重置]                     [应用]  [确定]  [取消]     │
└─────────────────────────────────────────────────────────────────┘
```

**添加供应商流程：** 预设选择（单选）→ 编辑页

- 预设按分类展示（余额型/套餐型/订阅型/可选）
- 支持搜索过滤
- 底部有"自定义配置"入口

**编辑页布局（复刻 cc-switch 表单模式）：**

```
┌── 编辑供应商 — MiniMax ────────────────────────────────────────┐
│                                                                  │
│  名称: [MiniMax                               ]                 │
│  URL:  [https://api.minimax.com/v1/usage       ]                │
│  信任模式: [▼ Strict]                                          │
│                                                                  │
│  ┌── 凭据 ──────────────────────────────────────────────────┐   │
│  │  API Key: [••••••••••••••••••••]  [从 KWallet 读取]      │   │
│  │  Access Token: [••••••••••••••••••••]  [从 KWallet 读取] │   │
│  │  User ID: [user_123                             ]          │   │
│  └───────────────────────────────────────────────────────────┘   │
│                                                                  │
│  ┌── 用量查询脚本 ──────────────────────────────────────────┐   │
│  │  来源: [▼ 内置预设（不可编辑，查看原文）]                    │   │
│  │  或: [▼ 自定义脚本]                                        │   │
│  │  ┌─────────────────────────────────────────────────────┐ │   │
│  │  │ ({ request: { ... }, extractor: function(r) {      │ │   │
│  │  │   return [ ... ]                                   │ │   │
│  │  │ } })                                                │ │   │
│  │  └─────────────────────────────────────────────────────┘ │   │
│  │                                           [测试运行] ▸    │   │
│  │  ┌── 测试结果预览 ───────────────────────────────────┐  │   │
│  │  │ ✓ 5小时窗口: 12% (12.5/100 USD) 4小时30分钟后重置  │  │   │
│  │  │ ✓ 7天窗口:  22% (78/100 USD)   周日 00:00重置     │  │   │
│  │  └──────────────────────────────────────────────────┘  │   │
│  └───────────────────────────────────────────────────────────┘   │
│                                                                  │
│  [默认值]  [重置]                     [应用]  [确定]  [取消]     │
└──────────────────────────────────────────────────────────────────┘
```

**表单项联动规则（复刻 cc-switch）：**

- 选择"内置预设" → 名称/URL/脚本自动填充且不可编辑
- 切换到"自定义脚本" → 名称/URL 可编辑，脚本区域可编辑
- 信任模式选择 → 切换安全规则提示
- 测试运行 → 使用当前参数（含凭据但不存储）执行完整查询，结果在预览区展示
- 不提供"添加维度"功能——维度由脚本返回决定

## 交互规范

### 点击与悬浮

- **鼠标左键单击 compact 圆球** → 弹出悬浮面板（fullRepresentation），非弹窗对话框
- **鼠标悬停 compact 圆球** → 显示 tooltip，内容为各供应商用量摘要，主要显示当前小工具显示的模型使用率，不要所有厂商都显示
- **悬浮面板外点击** → 面板关闭
- **固定按钮** → 切换面板 alwaysOnTop 模式

### 右键菜单样式切换（参考 KDE 6 系统监视器小部件）

**目标**：仿 KDE 6 系统监视器（如内存使用率）原生小部件的右键菜单，提供图表样式切换能力。

**菜单项**（按截图 + 规范，对应 `Plasmoid.contextualActions`）：

1. **打开系统监视器…** — 调用 `xdg-open <system-monitor>` 或 `plasma-systemmonitor`，便于跨查
2. **配置…** — `plasmoid.action("configure").trigger()`，打开 KCM 配置页（已实现）
3. **显示样式 ▸** — 子菜单，互斥单选（三选一）：
   - **饼状图**（PieChartControl，圆球 + full 面板默认）
   - **柱状图**（BarChartControl，full 面板所有 PlanBar）
   - **传感器详情**（`SensorChart`/`SensorFace` 或 quickcharts `LineChartControl`，按时间序列展示用量历史）
4. **刷新** — 立即刷新用量数据（已实现）

**实现细节**：

- 子菜单用 `PlasmaCore.Action` 嵌套实现，外层 `text: "显示样式"`，内层 3 个 action `checkable: true`、`autoExclusive: true`，通过 `cfg_displayStyle` 存储当前选择
- 选中的样式在 compact 圆球 + full 面板上**同时生效**：
  - `饼状图`:圆球用 `PieChartControl`，PlanBar 仍用 `BarChartControl`
  - `柱状图`:圆球退回纯文字百分比(无图)，PlanBar 用 `BarChartControl`
  - `传感器详情`:圆球用 `SensorFace`(带历史曲线)，PlanBar 区改为 `LineChartControl` 时间序列
- 配置项 `cfg_displayStyle` 加入 `config/main.xml` 的 `ui` 组（默认值 `"pie"`）
- 切换样式通过 KConfig 持久化，重启 plasmoid 后保持
- 切换时用 `NumberAnimation` / `PropertyAnimation` 做 200ms 平滑过渡，避免硬切

**图标**（右键菜单项的 icon.name）：

- 打开系统监视器：`utilities-system-monitor`
- 配置：`configure`
- 显示样式 ▸：`preferences-desktop-display`
- 饼状图：`office-chart-pie`
- 柱状图：`office-chart-bar`
- 传感器详情：`appointment-soon` 或 `view-object-historic-linear`
- 刷新：`view-refresh`

### 右键菜单

- 配置… — 打开 KCM 配置页
- 刷新 — 立即刷新用量数据

## 修复清单

1. ProviderGroup.qml：删除重复的 `border.color` 赋值（保留 switch 表达式）
2. 错误态可见性：`visible: errorText.length > 0`（不再要求 plans 为空数组）
3. 无 plan 时：`tightestUsedPercent()` 返回 -1，Orb 显示灰色 `"—"`
4. 移除 `configGeneral.qml` 中无效的 import
5. `qmllint` 在所有 .qml 文件上无错误

## 关键字

- **本次实现**：mock 数据 + Timer 驱动 + 5 个 bug 修复
- **本次不实现**：KConfig 完整读写、KWallet、QuickJS、HTTP 请求、KCM 配置页、真实供应商查询
- **兼容性**：mock 数据结构与未来 DisplayQuota 对齐，替换时只需换数据源，QML 层不变
- **供应商查询逻辑**：复用 cc-switch 现有实现，不重新发明

## 风险与缓解

| 风险                                   | 缓解                                     |
| -------------------------------------- | ---------------------------------------- |
| `plasmawindowed` 在 Wayland 下行为异常 | 先在 X11 验证，记录 Wayland 问题         |
| Timer 与 UI 刷新不同步                 | 直接赋值 `providers` 属性，触发 QML 绑定 |
| mock 数据与真实数据不匹配              | 按 extractor 返回格式设计，结构对齐      |


| 风险 | 缓解 |
|------|------|
| `plasmawindowed` 在 Wayland 下行为异常 | 先在 X11 验证，记录 Wayland 问题 |
| Timer 与 UI 刷新不同步 | 直接赋值 `providers` 属性，触发 QML 绑定 |
| mock 数据与真实数据不匹配 | 按 extractor 返回格式设计，结构对齐 |
