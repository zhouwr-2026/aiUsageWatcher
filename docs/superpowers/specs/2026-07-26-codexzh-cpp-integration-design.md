---
comet_change: codexzh-cpp-integration
role: technical-design
canonical_spec: openspec
archived-with: 2026-07-26-codexzh-cpp-integration
status: final
---

# CodexZH C++ 原生插件集成 — Technical Design

## Context

CodexZH 是已注册在内置 catalog 的供应商（providerRegistry.js 已有 logoSvg/website/plans），当前 plans 错误配置为"日限额/月限额"，实际为**周限额**，通过 **API Key 鉴权**查询。需实现完整的 C++ 原生插件：KWallet 凭证管理 + HTTP API 请求 + 响应解析 + applet 桥接 + QML 展示。

子 agent 已产出 C++ 代码骨架（stash@{1}），需提取并集成到 master。

同时，所有内置供应商的 Logo 需要固定在配置表单顶端，以 64x64 圆形头像显示，自定义供应商可点击选图。

## Goals / Non-Goals

**Goals:**
- CodexZH 真实周限额数据通过 C++ 原生插件查询并在 QML 面板展示
- API Key 通过 KWallet 安全存储（复用 MiniMax 的 KWallet 模式）
- 所有内置供应商 Logo 在 ProviderEditor 顶端 64x64 圆形头像固定显示
- 自定义供应商点击 Logo 头像弹出本地文件选择器修改 logo
- providerRegistry.js 中 codexzh 计划修正为周限额
- 移除内置供应商的 logoPath 输入框

**Non-Goals:**
- 不修改 providerSnapshot.js（C++ backend 直接推送 snapshot，如 MiniMax 模式）
- 不为内置供应商提供 logoPath 可编辑输入框
- 不做 OAuth 登录（CodexZH 仅 API Key 鉴权）
- 不改动供应商面板（ProviderGroup/PlanBar）的 logo 渲染逻辑（上次 change 已实现）

## Architecture

```
┌──────────────────────────────────────────────────────────────┐
│                      C++ Native Plugin                        │
│                                                              │
│  CodexZhClient                    CodexZhResponseParser       │
│  ┌──────────────────┐            ┌───────────────────────┐   │
│  │ KWallet           │            │ parse(QByteArrayView)  │   │
│  │  ├ saveCredential │            │  → CodexZhParseResult │   │
│  │  ├ clearCredential│            │    ├ .ok              │   │
│  │  └ loadCredential │            │    ├ .snapshot        │   │
│  │                   │            │    └ .errorMessage    │   │
│  │ QNetworkAccessMgr │            └───────────────────────┘   │
│  │  ├ refresh()      │                                        │
│  │  └ handleReply()  │                                        │
│  └──────────────────┘                                         │
│                                                              │
│  AiUsageWatcherApplet (bridge)                               │
│  ┌──────────────────────────────────────────┐                │
│  │ Q_PROPERTY(QVariantMap codexzhSnapshot)  │                │
│  │ Q_PROPERTY(bool codexzhLoading)          │                │
│  │ Q_PROPERTY(bool codexzhCredential*)      │                │
│  │ Q_INVOKABLE refreshCodexZhUsage()        │                │
│  │ Q_INVOKABLE saveCodexZhApiKey()          │                │
│  │ Q_INVOKABLE clearCodexZhApiKey()         │                │
│  └──────────────────────────────────────────┘                │
└──────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌──────────────────────────────────────────────────────────────┐
│                        QML Layer                              │
│                                                              │
│  main.qml                      ProviderEditor.qml            │
│  ┌────────────────────┐       ┌─────────────────────────┐   │
│  │ applyCodexZhSnapshot│      │ ┌──────────────────┐    │   │
│  │ requestCodexZhRefresh│     │ │ 64x64 SVG Logo   │    │   │
│  │ Connections          │     │ │ (内置固定/自定义选)│    │   │
│  └────────────────────┘       │ └──────────────────┘    │   │
│                                │ 名称: [________]        │   │
│  providerRegistry.js           │ URL:  [________]        │   │
│  ┌────────────────────┐       │ API Key: [________]     │   │
│  │ codexzh: 周限额     │       │ 或 [登录 Codex]         │   │
│  │ plans: [{weekly}]   │       └─────────────────────────┘   │
│  └────────────────────┘                                      │
└──────────────────────────────────────────────────────────────┘
```

## Decisions

### D1. 复用 MiniMax 的 KWallet + 网络模式

**为什么**：MiniMaxClient 已有完整的 KWallet（`AI Usage Watcher` 文件夹） + QNetworkAccessManager 异步请求 + credential 状态管理。CodexZH 的 API Key 鉴权与之完全一致，直接复用同一钱包路径和 entry 命名约定。

**接口对齐**：CodexZhClient 的 Q_PROPERTY 和 Q_INVOKABLE 签名与 MiniMaxClient 一致，减少配置表单的桥接代码量。

### D2. 使用子 agent 代码骨架，修正响应解析

**为什么**：stash@{1} 中 codexzhclient.cpp（414行）和 codexzhresponseparser.cpp（193行）结构完整，与 MiniMaxClient 模式一致。直接应用节省约 80% 工作量。

**需修正**：
- 响应解析中 plan 字段名匹配 CodexZH 实际 API 返回格式
- 补充周限额的 resetText 计算逻辑

### D3. ProviderEditor 顶端 Logo 头像布局

**为什么**：所有内置供应商的 Logo 来自 providerRegistry.js 的内联 SVG，无需用户配置。自定义供应商需要选图能力。

**布局方案**：
- 在 `basicForm` 的 GridLayout 之前插入独立行，跨两列
- 居中 64x64 圆形头像（`Rectangle { radius: width/2 }`）
- 内置供应商：`Image` 加载 `data:image/svg+xml;utf8,` + 内联 SVG，**不可点击**
- 自定义供应商：`MouseArea` 包裹 → 点击弹出 `FileDialog` → 选择后 `updateField("logoPath", url)`
- 失败时 fallback 到首字符

**移除**：GridLayout 中现有的 `FieldLabel { text: "Logo 路径" }` 行和 `providerLogoPathField`。

### D4. 认证输入差异

| 供应商类型 | catalogId | 表单区域 | 现有实现 |
|-----------|-----------|---------|---------|
| API Key | minimax / codexzh | API Key 输入框 + 保存/清除/刷新 | MiniMax 区域（520-620行） |
| OAuth | codex | 登录按钮 + 验证码 + 账号管理 | Codex 登录区域（364-513行） |
| 自定义 | custom / 空 | 脚本编辑器 + 限额项 | 自定义区域（622-902行） |

CodexZH 的 API Key 配置复用 MiniMax 的 API Key 区域 UI，注册时 `isCodexZh` 属性对应控制可见性。

### D5. providerRegistry 修正为周限额

**为什么**：当前 codexzh plans 为 `[{daily}, {monthly}]`，实际为周限额。

```js
// 改为
plans: [{ id: "weekly", planName: "周限额", unit: "%" }]
```

resetPeriodSec 保持 30*24*3600（与子 agent 代码一致，具体周期待 API 实测验证）。

## Risks / Trade-offs

- [R1] CodexZH API 端点/响应格式基于推测 → 子 agent 代码需用真实 API Key 测试验证
- [R2] KWallet 首次使用需用户授权弹窗 → 与 MiniMax 行为一致
- [R3] 内置 SVG 在 QML Image 中偶有渲染问题 → 单个 SVG 控制在 800 字以内，用 `#rrggbb` 颜色

## Test Strategy

- C++ 编译：`cmake --build build` 编译通过
- 安装测试：`kpackagetool6 --upgrade package` 无错误
- 视觉验证：配置表单打开确认 Logo 头像显示、布局正常
- 认证验证：API Key 保存/清除/刷新流程正确

## Migration Plan

1. 从 stash@{1} 提取 codexzhclient.cpp/h、codexzhresponseparser.cpp/h 到 src/
2. 修改 CMakeLists.txt 注册 4 个源文件 + 测试目标
3. 修改 aiusagewatcherapplet.cpp/h 添加 CodexZhClient 桥接（Q_PROPERTY + Q_INVOKABLE + signal）
4. 修改 main.qml 添加 CodexZH snapshot 接入（applyCodexZhSnapshot + requestCodexZhRefresh + Connections）
5. 修改 ProviderEditor.qml：顶端 Logo 头像 + 移除 logoPath 输入框 + 添加 `isCodexZh` 属性控制 API Key 区域
6. 修改 ProvidersConfig.qml：列表项 Logo 缩略图对齐
7. 修改 providerRegistry.js 修正为周限额
8. 编译验证 + 安装测试

## Open Questions

- [ ] CodexZH API 实际端点 URL 和 JSON 响应结构需真实 API Key 验证
- [ ] 周限额 resetPeriodSec 用 7 天还是 30 天？取决于 API 侧重置策略，先保持 30 天
