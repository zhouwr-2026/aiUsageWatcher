# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## 项目概览

AI Usage Watcher — KDE Plasma 6 桌面小部件，实时监控各大模型厂家（GLM、MiniMax、Codex、Gemini 等）的套餐用量。

## 构建与运行

项目包含 QML 界面和 C++ 原生查询后端：

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build

QT_PLUGIN_PATH="$HOME/.local/lib/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}" \
  plasmawindowed aiUsageWatcher
```

## 代码架构

```
package/                          # 小部件包根目录
├── metadata.json                 # KPlugin 元数据（Id: aiUsageWatcher）
└── contents/
    ├── config/
    │   ├── config.qml            # KCM ConfigModel 入口
    │   └── main.xml              # KConfig XT 配置文件
    ├── js/
    │   ├── mockData.js           # 种子数据、波动函数与数据派生
    │   └── providerConfig.js     # KCM 供应商配置校验逻辑
    └── ui/
        ├── main.qml              # 根组件（PlasmoidItem）
        ├── CompactView.qml       # compact 视图（pie/bar）
        ├── FullView.qml          # full 弹出面板
        ├── Orb.qml               # 圆球组件
        ├── PieChart.qml          # 自研饼图组件（Canvas）
        ├── ProviderGroup.qml     # 供应商卡片
        ├── PlanBar.qml           # 套餐进度条
        └── config/
            ├── GeneralConfig.qml   # 常规设置页
            ├── ProvidersConfig.qml # 供应商管理页
            └── ProviderEditor.qml  # 供应商编辑对话框
docs/
├── requirements.md               # 统一需求、设计与计划文档
└── usage-script-spec.md          # 自定义用量查询脚本规范
tests/
├── tst_mockData.qml              # 数据契约与派生逻辑测试
├── tst_compactView.qml           # compact 视图测试
├── tst_fullView.qml              # full 视图测试
├── tst_generalConfig.qml         # 常规设置测试
├── tst_providerConfig.qml        # 供应商配置测试
├── run-static-checks.sh          # 自动静态检查入口
├── run-plasma-smoke.sh           # 安装级冒烟测试入口
└── README.md                     # 测试说明
```

### 组件层级

```
PlasmoidItem (main.qml)
├── compactRepresentation: CompactView → Orb 风格圆球
└── fullRepresentation: FullView → Flickable → Column
    └── Repeater(providers) → ProviderGroup
        └── Repeater(plans) → PlanBar
```

### 数据流

- KConfig `providers` 只保存 `ProviderDefinition`（持久化）
- `mockData.js` 输出 `RuntimeProviderSnapshot`（仅内存）
- 纯函数 `buildDisplayProviders` 合并定义为展示模型
- compact 每 5 秒轮巡供应商，Timer 每 60 秒刷新数据

### 颜色语义

| `usedPercent` | 语义 | 颜色 |
|---|---|------|
| `< 85` | 正常 | `Kirigami.Theme.positiveTextColor` |
| `85..94` | 注意 | `Kirigami.Theme.neutralTextColor` |
| `>= 95` | 紧张 | `Kirigami.Theme.negativeTextColor` |
| 无数据 | 未知 | `Kirigami.Theme.disabledTextColor` |

### 关键 QML 组件属性

- **CompactView.qml**：`tightestUsage`（含 usedPercent/providerName/planName）
- **ProviderGroup.qml**：`providerId`、`providerName`、`ledClass`、`statusLabel`、`plans[]`、`errorText`
- **PlanBar.qml**：`planName`、`usedPercent`、`usedPercentLabel`、`usedText`、`totalText`、`unitText`、`resetText`、`extraText`、`templateText`

## 开发约定

- 供应商名自动剥 ` · <App>` 后缀（`stripProviderSuffix` 函数）
- `unit` 超 8 字符或含空白 → 放 `unitOverflow` 单独展示
- 进度条动画 300ms `Easing.OutCubic`
- 数据不可变更新（刷新函数返回新数组，不修改原对象）
- 需求基线以 `docs/requirements.md` 为准
- 自定义脚本规范以 `docs/usage-script-spec.md` 为准
- 代码风格加载 `ponytail`；简化历史代码用 `code-simplifier`

## 搜索提示

- `rg` 优先于 grep 搜索
- 字符串/日志搜索不用 `codegraph-usage`
- 涉及调用链、影响面、重构定位时加载 `codegraph-usage`（需 `.codegraph/` 目录存在）