# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概览

额度领航员（QuotaPilot，插件 ID `org.kde.plasma.AIQuotaPilot`）— KDE Plasma 6 桌面小部件，C++ 原生后端 + QML 界面，实时监控多家 AI 厂商的套餐/余额用量。Codex、MiniMax、DeepSeek、CodexZH、OpenCode Go、Agnes AI、Command Code 七家已接通原生凭据与查询；Claude Code、智谱 GLM、Kimi For Coding、硅基流动暂只显示暂无用量；自定义供应商走 HTTP + 独立 JS worker。

## 构建与运行

```bash
# 依赖：Qt 6.6+、Plasma 6、KF6 CoreAddons/Config/Wallet、CMake（不是 QML-only！）
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build

# 独立窗口运行（开发期，无需添加到面板）
QT_PLUGIN_PATH="$HOME/.local/lib/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}" \
  plasmawindowed AIQuotaPilot
```

- C++ 原生插件 `org.kde.plasma.AIQuotaPilot`（`src/`）提供全部网络/凭据能力；两个 worker 可执行文件（`quota-pilot-script-worker`、`quota-pilot-kwallet-worker`）随插件安装到 libexec
- 本机部署/重启 Shell：参考 `.agents/skills/plasma-local-deploy/SKILL.md`（该目录未入库，克隆后需自行重建）；重启用 `systemctl --user restart plasma-plasmashell.service`，不用 `plasmashell --replace`
- 验证：`bash tests/run-static-checks.sh`（QML 测试 + qmllint + 静态门禁）、`ctest --test-dir build`（C++ 测试）；安装级冒烟 `bash tests/run-plasma-smoke.sh`

## 代码架构

```
src/                                # C++ 原生后端（网络、凭据、解析、worker）
├── aiusagewatcherapplet.*          # Plasma::Applet：Codex 设备码/用量状态机 + 客户端接线
├── {agnes,minimax,deepseek,codexzh,opencodego,commandcode}client.*  # 六家同构客户端
├── credentialclientbase.*          # 六家客户端公共基类（KWallet 凭据 + 快照/限流）
├── *responseparser.*               # 各厂商响应解析与校验
├── kwalletdispatcher.* + kwalletworker.cpp   # KWallet 独立 worker（plasmashell 外）
├── customusageclient.* + usagescriptworker.cpp  # 自定义 HTTP+JS 查询（独立 worker 沙箱）
├── sharedproviderconfig.*          # 供应商定义共享配置（KConfig + 跨进程 watcher）
├── codexloginoutputparser.* / javascripthighlighter.*
package/contents/
├── config/main.xml                 # KConfig XT（全部字段见 docs/requirements.md §7）
├── js/
│   ├── providerCatalog.js          # 11 家预设目录（单一数据源：label/plans/vendor/logoPath）
│   ├── providerNormalize.js        # 定义归一化、种子快照、replaceSnapshot、usageClass
│   ├── displayProvider.js          # buildDisplay：定义+快照 → 展示模型（颜色/排序/stale 灰显）
│   ├── providerConfig.js / scriptTools.js / providerRegistry.js
└── ui/
    ├── main.qml                    # PlasmoidItem：快照接线、Timer、事件、tooltip
    ├── CompactView.qml             # compact 饼图/进度条
    ├── QuotaTooltip.qml            # 首个限额项文字 Tooltip
    ├── FullView.qml / PanelPieView.qml   # popup 柱状图/饼图两种布局
    ├── ProviderGroup.qml / PlanBar.qml   # 供应商卡片 / 限额进度条
    └── config/GeneralConfig.qml + ProvidersConfig.qml + ProviderEditor.qml
docs/requirements.md                # 需求与验收基线（唯一权威；改动须同步）
docs/usage-script-spec.md           # 自定义用量查询脚本安全契约
tests/                              # QML/JS 测试 + C++ 测试 + 静态/冒烟入口
```

### 数据流（双轨：定义走共享配置，快照走 C++ 属性）

- 定义：C++ `SharedProviderConfig`（KConfig 文件 `aiquotapilotrc`，组 `Providers/definitions`，KConfigWatcher 跨进程通知）→ `usageBackend.sharedProviders` → `providerNormalize.normalizeDefinitions` → 兜底旧 `Plasmoid.configuration.providers`
- 快照：每家已接通厂商一个 C++ Q_PROPERTY（`miniMaxSnapshot`/`deepseekSnapshot`/`codexSnapshot`/`codexzhSnapshot`/`opencodeGoSnapshot`/`agnesSnapshot`/`commandCodeSnapshot` + `customUsageSnapshots`）→ main.qml `applySnapshotFor` 按 `providerId` 匹配 → `runtimeSnapshots`
- 展示：`displayProvider.buildDisplay(definitions, snapshots, {sortMode, customOrderRaw})` → `providers`；纯函数、不可变更新
- 轮询：`refreshIntervalSec`（默认 60s）刷新所有厂商数据；`pollingIntervalSec`（默认 5s）轮巡 compact 显示；D-Bus `ModelActivated(QString)` 事件立即切换 + 高亮

### 组件层级

```
PlasmoidItem (main.qml)
├── compactRepresentation: CompactView（饼图/进度条）
└── fullRepresentation: FullView → Column → PanelPieView/柱状列表
    └── ProviderGroup（标题 + logo/LED + plans Repeater）
        └── PlanBar（计划名 + 进度条 + 百分比 + 模板文本 + extraText）
```

### 颜色语义（唯一口径：displayProvider.js `_usageClass`）

- `usedPercent < 85` → 正常（positive）；`85..94` → 注意（neutral）；`>= 95` → 紧张（negative）
- 无数据/无效/`stale`（上次刷新失败保留的旧值）→ `disabledTextColor`（灰），stale 数值保留但整卡灰显并标注“数据暂时不可更新”
- 颜色全部取自 Kirigami 语义色，禁硬编码 hex（CI 有 forbidden-patterns 门禁）
- `barClass`/`ledClass` 字符串（`bar-green` 等）由 JS 生成，QML 组件映射到 Kirigami 色

### 关键 QML 组件属性

- **main.qml**：`providerDefinitions`、`runtimeSnapshots`、`providers`、`compactUsage`（`providerUsageAt`）、`effectiveSortMode`；函数 `refresh()`/`applySnapshotFor()`/`activateModel()`
- **CompactView.qml**：`providers`、`compactStyle`（"pie"/"bar"）、`providerIndex`、`highlighted`、`plasmoidItem`；`currentUsage`（含 `stale`）
- **ProviderGroup.qml**：`providerName`、`ledClass`、`sourceLabel`、`statusLabel`、`plans[]`、`errorText`、`website`、`logoSource`、`logoChar`、`priceText`
- **PlanBar.qml**：`planName`、`usedPercent`、`usedPercentLabel`、`barClass`、`usedText`/`totalText`、`unitText`/`unitOverflow`、`resetText`、`extraText`、`templateText`、`usageSegments`
- 所有渲染供应商/远端文本的 Label 必须显式 `textFormat: Text.PlainText`（CI 依赖人工把关，无门禁）

## 开发约定

- 供应商名自动剥 ` · <App>` 后缀（`providerNormalize.stripProviderSuffix`）
- `unit` 超 8 字符或含空白 → 放 `unitOverflow` 单独展示
- 进度条动画 300ms `Easing.OutCubic`
- 数据不可变更新（刷新返回新数组/新对象）
- 刷新失败语义：无旧快照 → 清空显示“请求失败”；有旧快照 → 保留数值并标 `stale`（QML 灰显），不得以正常语义色冒充最新
- 预设厂商目录以 `providerCatalog.js` 为准（11 家 + custom）；新增预设需同步 catalog、main.xml 不涉及、requirements.md §2/§4
- 新增原生厂商 client：继承 `CredentialClientBase` 模式 + applet Q_PROPERTY/接线样板 + main.qml 的 apply/refresh/Connections 三处 + 需求文档
- 需求基线以 `docs/requirements.md` 为准；自定义脚本规范以 `docs/usage-script-spec.md` 为准
- 代码风格加载 `ponytail`；简化历史代码用 `code-simplifier`

## 搜索提示

- `rg` 优先于 grep 搜索
- 字符串/日志搜索不用 `codegraph-usage`
- 涉及调用链、影响面、重构定位时加载 `codegraph-usage`（需 `.codegraph/` 目录存在）
