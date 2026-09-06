# AGENTS.md

This file provides guidance to Codex (Codex.ai/code) when working with code in this repository.

## 项目概览

额度领航员（QuotaPilot，插件 ID `org.kde.plasma.AIQuotaPilot`）— KDE Plasma 6 桌面小部件，C++ 原生后端 + QML 界面，实时监控多家 AI 厂商（Codex、MiniMax、DeepSeek、CodexZH、OpenCode Go、Agnes AI、Command Code 已接通原生查询；Claude Code、智谱 GLM、Kimi For Coding、硅基流动暂无查询适配器）的套餐用量。用户可自定义供应商，其 `request`/`extractor` 脚本由独立 worker 进程沙箱执行。

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build

QT_PLUGIN_PATH="$HOME/.local/lib/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}" \
  plasmawindowed AIQuotaPilot
```

验证：`bash tests/run-static-checks.sh`（QML/JS 测试 + qmllint + 静态门禁）、`ctest --test-dir build-test`（C++ 测试）。

## 本机部署

- 生产代码修改完成后，先询问用户是否要重新编译、部署并重启 Plasma Shell；用户确认后使用项目内 `.agents/skills/plasma-local-deploy/SKILL.md`。
- **注意**：`.agents/` 未纳入 git（克隆新仓库后该技能不存在），此时内联执行同等步骤或让用户重建该目录，不要声称技能存在。
- 重启 Shell 用 `systemctl --user restart plasma-plasmashell.service`；禁止用 DBus `refreshCurrentShell` 或终端后台运行 `plasmashell --replace`。

## KDE开发者网站

地址：<https://develop.kde.org/docs/plasma/widget/examples/>

## 代码架构

```
src/                          # C++ 原生后端（全部网络/凭据/KWallet/worker 在此，QML 只读去敏快照）
├── aiusagewatcherapplet.*    # Plasma::Applet：Codex 设备码登录/用量状态机 + 六家 client 接线样板
├── {agnes,minimax,deepseek,codexzh,opencodego,commandcode}client.*
├── credentialclientbase.*    # 六家同构客户端公共基类（凭据 + 快照 + stale/限流语义）
├── *responseparser.*         # 响应解析与 qint64/结构校验
├── kwalletdispatcher.* / kwalletworker.cpp    # KWallet 独立 worker 进程
├── customusageclient.* / usagescriptworker.cpp  # 自定义 HTTP+JS 查询沙箱
├── sharedproviderconfig.*    # 供应商定义共享 KConfig + KConfigWatcher
└── codexloginoutputparser.* / javascripthighlighter.*
package/contents/
├── metadata.json             # KPlugin 元数据（Id: org.kde.plasma.AIQuotaPilot）
├── config/main.xml           # KConfig XT（字段表见 docs/requirements.md §7）
├── js/
│   ├── providerCatalog.js    # 11 家预设目录（label/plans/vendor/logoPath 单一数据源）
│   ├── providerNormalize.js  # 定义归一化/种子快照/usageClass（85/95 阈值）
│   ├── displayProvider.js    # buildDisplay：定义+快照 → 展示模型（排序/stale 灰显/PAYG）
│   └── providerConfig.js / scriptTools.js / providerRegistry.js
└── ui/
    ├── main.qml              # 数据流、刷新、轮询、D-Bus 事件接线
    ├── CompactView.qml / QuotaTooltip.qml
    ├── FullView.qml / PanelPieView.qml / ProviderGroup.qml / PlanBar.qml
    └── config/GeneralConfig.qml + ProvidersConfig.qml + ProviderEditor.qml
docs/
├── requirements.md           # 需求与验收基线（改动代码须同步此处）
├── usage-script-spec.md      # 自定义用量查询脚本安全契约
tests/                        # tst_*.qml + cpp/ 12 个 C++ 测试 + 静态/冒烟入口
```

### 组件层级

```
PlasmoidItem (main.qml)
├── compactRepresentation: CompactView（饼图/进度条，配置可选）
└── fullRepresentation: FullView → Column → ScrollView/PanelPieView
    └── ProviderGroup（标题 + logo/LED + plans Repeater）
        └── PlanBar（计划名 + 进度条 + 百分比 + 模板/extra 文本）
```

### 数据流（双轨）

- 定义（持久化）：`SharedProviderConfig`（`aiquotapilotrc` `Providers/definitions`）→ `usageBackend.sharedProviders` → `normalizeDefinitions`；旧 `Plasmoid.configuration.providers` 仅作迁移兜底
- 快照（仅内存）：每家厂商一个 C++ Q_PROPERTY（如 `miniMaxSnapshot`）+ `customUsageSnapshots` → main.qml `applySnapshotFor(backendKey, providerId)` 按 providerId 匹配入 `runtimeSnapshots`
- 展示：纯函数 `buildDisplay(definitions, snapshots, {sortMode, customOrderRaw})` → `providers`
- 刷新节奏：`refreshIntervalSec`（60s，10..3600）刷新全部数据；`pollingIntervalSec`（5s，1..300）轮巡 compact；D-Bus `ModelActivated(QString)` 事件切换 + 高亮（`highlightDurationSec`）
- 失败语义：无旧快照 → 清空“请求失败”；有旧快照 → 保留并标 `stale`（QML 灰显 + “数据暂时不可更新”），不得以正常语义色冒充最新

### 颜色语义

| `usedPercent` | 语义 | 颜色 |
| --- | --- | --- |
| `< 85` | 正常 | `Kirigami.Theme.positiveTextColor` |
| `85..94` | 注意 | `Kirigami.Theme.neutralTextColor` |
| `>= 95` | 紧张 | `Kirigami.Theme.negativeTextColor` |
| 无数据 / invalid / stale | 未知 | `Kirigami.Theme.disabledTextColor` |

禁止硬编码 hex 主题色（CI forbidden-patterns 门禁）。

### 关键 QML 组件属性

- **main.qml**：`providerDefinitions`、`runtimeSnapshots`、`providers`、`compactUsage`（含 `stale`）、`effectiveSortMode`
- **CompactView.qml**：`providers`、`compactStyle`（"pie"/"bar"）、`providerIndex`、`highlighted`、`plasmoidItem`
- **ProviderGroup.qml**：`providerName`、`ledClass`、`sourceLabel`、`statusLabel`、`plans[]`、`errorText`、`website`、`logoSource`、`logoChar`、`priceText`
- **PlanBar.qml**：`planName`、`usedPercent`、`usedPercentLabel`、`barClass`、`usedText`/`totalText`、`unitText`/`unitOverflow`、`resetText`、`extraText`、`templateText`、`usageSegments`

## 开发约定

- 供应商名自动剥 `· <App>` 后缀（`stripProviderSuffix`）
- `unit` 超 8 字符或含空白 → 放 `unitOverflow` 单独展示
- 进度条动画 300ms `Easing.OutCubic`；数据不可变更新
- 渲染供应商/远端文本的 Label 必须 `textFormat: Text.PlainText`
- 新增原生厂商 client 的样板：client 类（继承 `CredentialClientBase`）+ applet 属性/信号接线 + main.qml `applySnapshotFor`/`requestRefreshFor`/`Connections` 三处 + 需求文档 §2/§4
- 需求基线以 `docs/requirements.md` 为准；自定义脚本规范以 `docs/usage-script-spec.md` 为准
- 代码风格加载 `ponytail`；简化历史代码用 `code-simplifier`

## 搜索提示

- `rg` 优先于 grep 搜索
- 字符串/日志搜索不用 `codegraph-usage`
- 涉及调用链、影响面、重构定位时加载 `codegraph-usage`（需 `.codegraph/` 目录存在）
