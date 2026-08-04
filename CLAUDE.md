# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概览

AI Usage Watcher — KDE Plasma 6 桌面小部件，实时监控各大模型厂家（GLM、MiniMax、Claude、Codex、Gemini 等）的套餐用量。

## 构建与运行

```bash
# 安装到本地 Plasma 小部件目录
cp -r package ~/.local/share/plasma/plasmoids/org.kde.plasma.AIQuotaPilot/

# 用 plasmawindowed 独立窗口运行（开发期，无需添加到面板）
plasmawindowed AIQuotaPilot

# 安装到面板（打包）
kpackagetool6 --install AIQuotaPilot

# 卸载
kpackagetool6 --remove AIQuotaPilot
```

- 无需 C++ 编译（QML-only），CMakeLists.txt 仅用于包管理器识别
- 验证：`plasmawindowed AIQuotaPilot` 启动后检查小图标（柱状图/饼图）显示、颜色语义、点击展开、Timer 刷新

## 代码架构

```
package/                          # 小部件包根目录
├── metadata.json                 # KPlugin 元数据（Id: org.kde.plasma.AIQuotaPilot）
└── contents/
    ├── config/main.xml           # KConfig XT 配置文件（providerCount, opacityPercent, alwaysOnTop）
    ├── js/mockData.js            # 种子数据 + 波动函数（开发期 mock，Timer 驱动）
    └── ui/
        ├── main.qml              # 根组件（PlasmoidItem）：compactRepresentation + fullRepresentation
        ├── CompactView.qml        # 小图标（柱状图/饼图，配置可选）
        ├── ProviderGroup.qml     # 供应商卡片：标题 + LED 灯 + 多条 PlanBar
        ├── PlanBar.qml           # 单条水平进度条（计划名 + 进度条 + 百分比 + 重置时间）
        └── configGeneral.qml     # 配置页入口（占位，待实现）
docs/
├── requirements.md               # 核心需求基线（视觉规格、数据后端、安全要求、验收清单）
└── usage-script-spec.md          # 自定义用量查询脚本规范（extractor 返回格式）
openspec/changes/minimal-viable-plasmoid/  # 当前开发变更（Comet 流程）
```

### 组件层级

```
PlasmoidItem (main.qml)
├── compactRepresentation: CompactView（柱状图/饼图，配置可选；显示最紧张供应商）
└── fullRepresentation: FullView → Column → ScrollView
    └── ListView(providers) → ProviderGroup（标题 + Logo + LED 灯 + plans Repeater）
        └── Repeater(plans) → PlanBar（计划名 + 进度条 + 百分比 + 重置信息）
```

### 数据流

- `mockData.js` 提供 `SEED_PROVIDERS`（mock 数据）和 `fluctuateProviders()`（波动函数）
- `main.qml` 的 `Timer` 每 60s 触发 `fluctuateProviders()` 刷新数据
- 供应商数据含 `providerName`、`plans[]`、`errorText`、`ledClass`、`sourceLabel`、`statusLabel`
- 每个 plan 含 `planName`、`usedPercent`、`usedPercentLabel`、`barClass`、`resetText` 等

### 颜色语义（按已用 % 阈值）

- `≤5%` → 红色 `#f87171`（紧张）
- `>5%` 且 `≤15%` → 黄色 `#fbbf24`（注意）
- `>15%` → 绿色 `#34d399`（正常）
- 无数据 → 灰色 `#6b7280` / `#9ca3af`

### 关键 QML 组件属性

- **CompactView.qml**：`providers`、`compactStyle`（"pie"/"bar"）、`providerIndex`、`highlighted`、`plasmoidItem`
- **ProviderGroup.qml**：`providerName`、`ledClass`、`sourceLabel`、`statusLabel`、`plans[]`、`errorText`
- **PlanBar.qml**：`planName`、`usedPercent`、`usedPercentLabel`、`barClass`、`resetText`、`usedText`、`unitText`、`extraText`

## 开发约定

- 供应商名自动剥 ` · <App>` 后缀（`stripProviderSuffix` 函数）
- `unit` 超 8 字符或含空白 → 放 `unitOverflow` 单独展示
- 进度条动画 300ms `Easing.OutCubic`
- 数据不可变更新（`fluctuateProviders` 返回新数组，不修改原对象）
- 需求基线以 `docs/requirements.md` 为准
- 自定义脚本规范以 `docs/usage-script-spec.md` 为准
- 代码风格加载 `ponytail`；简化历史代码用 `code-simplifier`

## 搜索提示

- `rg` 优先于 grep 搜索
- 字符串/日志搜索不用 `codegraph-usage`
- 涉及调用链、影响面、重构定位时加载 `codegraph-usage`（需 `.codegraph/` 目录存在）