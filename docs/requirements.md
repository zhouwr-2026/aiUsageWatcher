# AI 用量监控 — 需求汇总

> 本文档是 aiUsageWatcher 的**核心需求基线**。
> 数据来源：
> 1. 上一轮 `ai-desktop-pet` 项目的需求文档（docs/requirements.md + docs/superpowers/specs/2026-07-19-usage-monitor-handoff.md）
> 2. 本轮 KDE Plasma 6 重写期间用户累积的所有要求（2026-07-20 ~ 2026-07-21）
> 3. KDE 开发者站（develop.kde.org）Plasma 6 小部件规范
>
> 任何冲突以此文档为准。

## 1. 项目目标

KDE Plasma 6 桌面小部件，**实时监控各大模型厂家的模型套餐用量**。常驻桌面 / 面板，单击 compact 图标展开为**紧贴小部件的悬浮面板**（非独立弹窗）。

### 1.1 形式（KDE Plasma 6 小部件）

- 安装目录：`~/.local/share/plasma/plasmoids/aiUsageWatcher/`
- 包结构：`package/metadata.json` + `package/contents/{ui,config}/`
- 根 QML：`PlasmoidItem`（Plasma 6 强制要求）
- 运行命令（开发期）：`plasmawindowed aiUsageWatcher`
- 安装命令：`kpackagetool6 --install aiUsageWatcher`
- 卸载：`kpackagetool6 --remove aiUsageWatcher`
- Plasma API 版本：`X-Plasma-API-Minimum-Version: "6.0"`
- 包结构：`KPackageStructure: "Plasma/Applet"`
- **必须严格按 KDE 6 原生技术栈开发**（2026-07-21 硬约束）
  - KCM 用 `org.kde.kcmutils` + `KCM.SimpleKCM` / `KCM.GridView`
  - 表单用 `org.kde.kirigami` 控件 + `QtQuick.Controls` 原生
  - 不引入自定义按钮/标签/分隔符，凡 KDE/Kirigami 提供的都优先用
  - 图标全部走 Breeze 图标主题（`icon.name: "..."`），不写自定义 SVG
  - 颜色取自 `Kirigami.Theme`（PlasmaCore.Theme 只有 uppercase ColorRole 枚举，没有 lowercase 颜色属性）

### 1.2 视觉规格

compact（面板/小尺寸）的显示内容是**可配置的图表**，不是固定圆球或文字：

| 状态 | 表现 |
|------|------|
| compact（面板/小尺寸） | 根据 KCM 配置显示 **饼型图** 或 **水平柱状图**，默认饼型图；下方不显示文字标签；单击展开为悬浮面板 |
| full（弹出面板） | 半透明深色背景，圆角；标题栏 + 可滚动供应商列表 + 状态栏 |
| 标题栏 | "AI 用量监控" + 刷新按钮 + 配置按钮 + 固定按钮 |

饼型图模式下：compact 区域显示 PieChart 饼图（用量占比环）
柱状图模式下：compact 区域显示水平填充矩形（进度条）

### 1.3 颜色语义（按已用 % 阈值，从 Kirigami.Theme 取色）

| 阈值 | 语义 | Kirigami 属性 |
|------|------|---------------|
| ≤5% | 红（紧张） | `Kirigami.Theme.negativeTextColor` |
| ≤15% | 黄（注意） | `Kirigami.Theme.highlightColor` |
| >15% | 绿（正常） | `Kirigami.Theme.positiveTextColor` |
| 无数据 | 灰 | `Kirigami.Theme.neutralTextColor` |

## 2. 交互规范（2026-07-21 用户确认）

### 2.1 点击与悬浮

- **左键单击 compact 图标** → 弹出悬浮面板（fullRepresentation），非弹窗对话框
- **鼠标悬停 compact 图标** → 显示 tooltip，内容为当前小工具显示的主模型使用率，不要所有厂商都显示
- **悬浮面板外点击** → 面板关闭
- **固定按钮** → 切换面板 alwaysOnTop 模式

### 2.2 右键菜单

- 仅有 2 项（用户硬约束：「打开系统监视器」「传感器详情」「显示样式」子菜单均与本小部件无关，已删除）
  - **配置…** — 打开 KCM 配置页
  - **刷新** — 立即刷新用量数据

### 2.3 显示样式切换

- 不在右键菜单中，**在 KCM 常规设置表单里**（2026-07-21 用户确认）
- 选项：**饼型图** / **水平柱状图**，默认饼型图
- 紧凑显示（compact）和全屏显示（full）的样式切换细节：
  - 饼型图模式：compact 图标显示 PieChart 饼图；full 面板用水平排布 PieChart 显示每个 plan
  - 柱状图模式：compact 图标显示水平填充矩形；full 面板用垂直堆叠 Rectangle 进度条显示每个 plan

### 2.4 面板布局规则

- **PlanBar 区域布局和样式和改动前完全一致**（2026-07-21 用户硬约束：不修改 PlanBar 区域布局）
- 每个 PlanBar 始终使用 `Rectangle` 宽度动画（300ms `Easing.OutCubic`）
- 面板右上角按钮：刷新 / 配置 / 固定（共 3 个按钮，无饼型/柱状切换按钮）

### 2.5 主题跟随

- 所有颜色取自 `Kirigami.Theme`，不硬编码十六进制颜色
- 紧凑显示（compact）和全屏显示（full）均自动跟随系统 Plasma 亮/暗主题
- 禁止使用 `PlasmaCore.Theme.xxx`（PlasmaCore 只有 `ColorRole` 枚举 + `color()` 方法，没有 lowercase 属性）

## 3. 数据架构

### 3.1 数据结构

```typescript
// 每个供应商对象
{
  "providerName": string,    // 供应商名称，如"云之声Token Hub"
  "ledClass": string,        // "led-green" | "led-yellow" | "led-red" | "led-gray"
  "sourceLabel": string,     // 来源标签，如"自定义"、"套餐"、"订阅"
  "statusLabel": string,     // 状态标签，如"可用"、"降级"
  "errorText": string,       // 错误信息，为空时不显示
  "template": string,        // 模板字符串，默认 "%1 限额  %2/%3  重置于 %4"
  "plans": [{
    "planName": string,      // 计划名，如"5小时"、"7天"、"30天"
    "usedPercent": number,   // 已用百分比 0-100
    "usedPercentLabel": string, // 百分比标签，如"65%"
    "barClass": string,      // "bar-green" | "bar-yellow" | "bar-red"
    "resetText": string,     // 重置时间文本，如"今天 18:00"
    "usedText": string,      // 已用量文本，如"141775516 / 180000000"
    "unitText": string,      // 单位文本，如"$"
    "extraText": string      // 额外说明文本
  }]
}
```

### 3.2 模板字符串机制（2026-07-21 用户要求）

- 每个 plan 的显示文本由 **模板字符串** 控制
- 默认模板：`"%1 限额  %2/%3  重置于 %4"`
- 占位符含义：
  - `%1` = planName（限额名，如"5小时"）
  - `%2` = used（已用，如 "141775516"）
  - `%3` = total（总量，如 "180000000"，当前 mock 数据中暂缺，传空字符串）
  - `%4` = resetIn 或 resetAt（重置时间）
- KCM 编辑页提供模板字符串字段可编辑 + 实时预览
- 渲染使用 `i18n()`（非 `i18np()`，`i18np` 是复数形式，需要整型 count 参数）

### 3.3 种子供应商

| 供应商 | 维度 | 状态 | 颜色 | 特点 |
|--------|------|------|------|------|
| 云之声Token Hub | 5h(65%) / 7d(22%) / 30d(8%) | 可用 | 🟢 | 多 PlanBar、多重置时间 |
| MiniMax | 余额(12%) | 降级 | 🟡 | 黄色阈值、extraText |
| Codex | 周限额(67%) 503/750次 | 可用 | 🟢 | 次/周单位、usedText |

### 3.4 数据持久化

- 非敏感配置：KConfig（通过 `plasmoid.configuration` 访问）
- 供应商列表（复杂对象数组）用 JSON 字符串存到 `plasmoid.configuration.providers`
- 启动时 fallback 到 `MockData.SEED_PROVIDERS`
- 任何修改（增/删/改/refresh）后调用 `plasmoid.configuration.providers = JSON.stringify(providers)`

## 4. KCM 配置页（2026-07-21 用户确认：全量实现增删改）

### 4.1 KCM 架构

- 顶层：`KCM.SimpleKCM`（自动获得标准底部按钮：默认值/重置/应用/确定/取消）
- 左侧导航：`Kirigami.NavigationTabBar`，2 个 Tab
  - **常规** — 常规设置表单
  - **供应商** — 供应商列表 + 增删改入口

### 4.2 常规设置表单（5 个表单项）

| 字段 | 控件 | 值绑定 | 默认值 |
|------|------|--------|--------|
| 图标样式 | ComboBox | `plasmoid.configuration.compactStyle` | 饼型图（"pie"） |
| 刷新间隔(秒) | SpinBox | `plasmoid.configuration.refreshIntervalSec` | 60 |
| 背景透明度 | Slider | `plasmoid.configuration.opacityPercent` | 80% |
| 窗口置顶 | CheckBox | `plasmoid.configuration.alwaysOnTop` | true |
| 默认分组 | ComboBox | `plasmoid.configuration.groupBy` | 按供应商 |
| 主题 | ComboBox | `plasmoid.configuration.colorScheme` | 跟随系统 |

### 4.3 供应商列表（ProvidersConfig）

- 列表行 = **供应商标题** + **展开的计划区**
- 标题行：LED 灯 + 名称 + [✏️ 编辑] [🗑 删除] 两个图标按钮
- 计划区：缩进 16px，垂直堆叠，每行 PlanBar 显示"限额名 + 进度条 + 百分比 + 重置时间"
- 添加按钮 → 弹出编辑对话框（空白表单）
- 编辑按钮 → 同对话框（预填当前供应商）
- 删除按钮 → 二次确认弹窗（Yes/No）→ 从数组移除并 persist

### 4.4 供应商编辑对话框（ProviderEditConfig）

| 字段 | 控件 | 说明 |
|------|------|------|
| 名称 | TextField | 供应商名称 |
| 来源 | ComboBox | 自定义 / 套餐 / 订阅 |
| 信任模式 | ComboBox | Strict / Lan / Custom |
| 计划 | 多行列表（可增删） | 每行：名称、限额文本、重置时间 |
| 模板字符串 | TextField | 默认 `"%1 限额  %2/%3  重置于 %4"` |
| 用量查询脚本 | 只读显示 | 提示"脚本编辑将在后续版本实现" |

### 4.5 config/main.xml 字段

```xml
<group name="ui">
    <entry name="refreshIntervalSec" type="int"> <default>60</default> <min>10</min> <max>3600</max> </entry>
    <entry name="opacityPercent" type="int"> <default>80</default> <min>20</min> <max>100</max> </entry>
    <entry name="alwaysOnTop" type="bool"> <default>true</default> </entry>
    <entry name="compactStyle" type="string"> <default>pie</default> </entry>
    <entry name="groupBy" type="string"> <default>provider</default> </entry>
    <entry name="colorScheme" type="string"> <default>default</default> </entry>
</group>
<group name="providers">
    <entry name="providerCount" type="int"> <default>0</default> </entry>
    <entry name="providers" type="string"> <default></default> </entry>
</group>
```

## 5. 文件结构

```
package/contents/
├── config/main.xml                    # KConfig XT 配置文件
├── js/mockData.js                     # 种子数据 + 波动函数
└── ui/
    ├── main.qml                       # PlasmoidItem 根组件
    ├── CompactView.qml                # 小工具图标视图（pie/bar 切换）
    ├── FullView.qml                   # 悬浮面板（3 个标题栏按钮 + 供应商列表）
    ├── PieChart.qml                   # 自研 Canvas 饼图组件
    ├── PlanBar.qml                    # 进度条组件（不动）
    ├── ProviderGroup.qml              # 供应商卡片组件（不动）
    └── config/
        ├── configConfig.qml           # KCM 顶层
        ├── GeneralConfig.qml          # 常规设置表单
        ├── ProvidersConfig.qml        # 供应商列表 + 增删改入口
        └── ProviderEditConfig.qml     # 单供应商编辑页（对话框）
```

## 6. 技术选型与约束

### 6.1 KDE 6 原生技术栈

- QML（Qt 6 / Plasma 6），无 C++ 编译
- `Kirigami.Theme` 取色（非 `PlasmaCore.Theme`）
- `KCM.SimpleKCM` + `Kirigami.NavigationTabBar`
- `QtQuick.Controls` 原生控件（SpinBox/Slider/ComboBox/CheckBox）
- Breeze 图标主题（`icon.name: "..."`）

### 6.2 图表组件

- **不依赖 `org.kde.quickcharts`**（2026-07-21 实测：Manjaro 当前包命名 bug，`qmldir` 引用 `plugin QuickChartsControlsplugin`，磁盘只有 `libQuickChartsControlsplugin.so`，QML loader 找不到，且部分场景触发 SIGSEGV）
- 改用**自研纯 QML Canvas 组件**：
  - `PieChart.qml` — 饼图/环形图，取色自 `Kirigami.Theme`
  - `LineChart.qml` — 折线图（当前未使用，文件可直接删除）
- 性能：Canvas 重绘 < 1ms/组件

### 6.3 供应商数据持久化

- KConfig XT 不支持复杂对象数组 → JSON 字符串存到 `plasmoid.configuration.providers`
- 启动时 fallback 到 `MockData.SEED_PROVIDERS`
- 任何修改后调用 `persistProviders()`

## 7. 验收清单

- [ ] `plasmawindowed aiUsageWatcher` 启动正常，compact 显示饼图
- [ ] 点击 compact 图标弹出面板，标题栏右侧看到 3 个按钮（刷新 / 配置 / 固定）
- [ ] 刷新按钮 → 图标转圈 300ms + 数据重新波动
- [ ] 配置按钮 → 打开 KCM，左侧 2 个 Tab（常规/供应商）
- [ ] 常规 Tab 修改字段 → 应用 → 重启后字段保持
- [ ] 供应商 Tab 添加/编辑/删除供应商 → 重启后保持
- [ ] 右键菜单只有 2 项：配置…、刷新
- [ ] KCM 常规设置"图标样式"下拉框切换饼型/柱状 → 应用 → compact 按所选渲染
- [ ] KCM 常规设置"主题"下拉框切换跟随系统/亮色/暗色 → 应用 → 全局 KDE 主题色切换生效
- [ ] `qmllint` 在所有 .qml 文件上 0 错误
- [ ] PlanBar 区域布局和样式和改动前完全一致
- [ ] 颜色随 Plasma 主题（亮/暗）切换而变化
- [ ] 供应商列表展开显示多条计划（5小时/7天/月限额）
- [ ] 计划区使用模板字符串渲染（`%1 限额  %2/%3  重置于 %4`）

## 8. 不在本次范围

- 真实的 HTTP 用量查询（本次继续用 mock 数据）
- KCM 编辑页的脚本编辑/凭据管理（只读显示）
- 历史 buffer / 时间序列图
- 系统监视器集成
- 供应商导入/导出