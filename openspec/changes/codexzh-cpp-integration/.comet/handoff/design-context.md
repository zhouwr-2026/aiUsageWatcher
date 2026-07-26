# Comet Design Handoff

- Change: codexzh-cpp-integration
- Phase: design
- Mode: compact
- Context hash: ce5e6ed76fcd020473f09483ecc8ce5b110d901dcec8f2b4bfc4b51bb76539ab

Generated-by: comet-handoff.sh

OpenSpec remains the canonical capability spec. This handoff is a deterministic, source-traceable context pack, not an agent-authored summary.

## openspec/changes/codexzh-cpp-integration/proposal.md

- Source: openspec/changes/codexzh-cpp-integration/proposal.md
- Lines: 1-28
- SHA256: f517b94422c938ea0aca8177e7733bace2f7dd6843febda0396deb08efaaecdf

```md
## Why

AIQuotaPilot 已支持 MiniMax（KWallet + HTTP API）、Codex（OAuth）和自定义脚本三种供应商的真实额度查询。CodexZH 作为注册在内置 catalog 中的供应商（providerRegistry.js 已有 logo/website/plans），其额度数据目前只能通过 QML 层 mock 填充，缺少 C++ 原生插件实现真实 API 查询。

需要为 CodexZH 添加完整的 C++ 原生插件：KWallet 凭证管理 + HTTP API 请求 + 响应解析 + applet 桥接 + QML 面板展示，使其与 MiniMax 一样可配置 API Key 并显示真实周限额数据。

## What Changes

- 新增 `src/codexzhclient.cpp/h`：KWallet 凭证存取 + QNetworkAccessManager HTTP 请求 + 凭证状态管理
- 新增 `src/codexzhresponseparser.cpp/h`：CodexZH API 响应 JSON 解析，返回周限额数据
- 修改 `src/aiusagewatcherapplet.cpp/h`：添加 CodexZhClient 成员、Q_PROPERTY 暴露、Q_INVOKABLE 方法、signal 桥接
- 修改 `CMakeLists.txt`：注册 4 个新源文件
- 修改 `package/contents/ui/main.qml`：添加 `codexzhSnapshot` 读取和 `refreshCodexZhUsage` 调用
- 修改 `package/contents/js/providerRegistry.js`：将 codexzh 计划从"日限额/月限额"修正为"周限额"

## Capabilities

### New Capabilities
- `codexzh-native-client`: CodexZH C++ 原生客户端，支持 KWallet API Key 管理 + HTTP API 查询 + JSON 响应解析，数据通过 applet Q_PROPERTY 桥接到 QML 面板显示

### Modified Capabilities
（无已有 capability 变更）

## Impact

- 新增 4 个 C++ 源文件（~700 行）
- 修改 4 个现有文件（CMakeLists.txt、applet .cpp/.h、main.qml、providerRegistry.js）
- 无新依赖（KWallet/QNetworkAccessManager 已在 MiniMax 中使用）
- 需要 cmake 重新编译```

## openspec/changes/codexzh-cpp-integration/design.md

- Source: openspec/changes/codexzh-cpp-integration/design.md
- Lines: 1-82
- SHA256: f7957feb4e43ec6fc4ff7a54d497ffbef70cb8ab136fc3422556f89b052631dc

[TRUNCATED]

```md
## Context

CodexZH 是已注册在内置 catalog 的供应商（providerRegistry.js 已有 logo/website/plans），当前只有"日限额/月限额"的 mock 计划占位。其真实 API 通过 API Key 鉴权，返回**周限额**数据。本项目已有 MiniMax 的 KWallet + QNetworkAccessManager 完整模式可供参考。

子 agent 已产出完整 C++ 代码骨架（codexzhclient.cpp/h、codexzhresponseparser.cpp/h、applet 桥接），需验证并集成到 master。

## Goals / Non-Goals

**Goals:**
- CodexZH 真实周限额数据通过 C++ 原生插件查询并在 QML 面板展示
- API Key 通过 KWallet 安全存储（复用 MiniMax 的 KWallet wallet/folder 命名约定）
- 与现有 MiniMax/Codex/custom 三套体系一致的 credential 状态管理（loading/configured/error）
- providerRegistry.js 中 codexzh 计划从"日限额/月限额"修正为"周限额"
- 无新依赖（KWallet/Qt Network 已引入）
- 所有内置供应商的 Logo 在配置表单顶端以 64x64 头像图片显示，固定不可编辑
- 自定义供应商点击顶端 Logo 头像弹出本地图片选择器修改 logo

**Non-Goals:**
- 不涉及配置页 UI 的 API Key 输入框（复用 MiniMax 模式的 ProvidersConfig/ProviderEditor）
- 不做 OAuth 登录（CodexZH 仅 API Key 鉴权）
- 不修改 providerSnapshot.js（C++ backend 直接推送 snapshot，如 MiniMax 模式）
- 不为内置供应商提供 logoPath 输入框（logo 内联固定，无可配置性）

## Decisions

### D1. 复用 MiniMax 的 KWallet 模式
**为什么**：项目已有 KWallet + QNetworkAccessManager 的 MiniMax 客户端完整模式。CodexZH 的 API Key 鉴权与之完全一致，复用钱包路径 `AI Usage Watcher`、entry 命名 `CodexZH API Key` 和异步 wallet 打开流程。不需要引入新依赖。

### D2. 使用已有子 agent 代码骨架
**为什么**：子 agent 已产出完整的 codexzhclient（414行）和 response parser（193行），经过初步审查结构合理，与 MiniMaxClient 模式一致。直接应用可节省约 80% 工作量。需修正以下问题：
- 响应解析中的 plan 字段名需匹配 CodexZH 实际 API 返回格式
- 需补充周限额的 resetPeriodSec 逻辑（当前 parser 未处理 resetText）

### D3. QML 侧以 MiniMax 模式接入
**为什么**：main.qml 已对 MiniMax 有 `applyMiniMaxSnapshot()` + `requestMiniMaxRefresh()` + `Connections` 的完整模式。为 CodexZH 添加平行的一套 `applyCodexZhSnapshot()` / `requestCodexZhRefresh()` / `Connections` 函数，通过 `usageBackend["codexzhSnapshot"]` 和 `usageBackend["refreshCodexZhUsage"]` 桥接。

### D4. providerRegistry.js 修正为周限额
**为什么**：CodexZH 实际只有周限额，而非日/月。修改 plans 数组为单个 `{ id: "weekly", planName: "周限额", unit: "%" }`，resetPeriodSec 保持 30*24*3600（或改为 7*24*3600 周周期——需确认 API 侧实际重置周期）。

### D5. 内置供应商 Logo 固定在配置表单顶端
**为什么**：所有内置供应商的 Logo 由 providerRegistry.js 的内联 SVG 提供，在配置表单（ProviderEditor）顶端以 64x64 圆形头像展示。不显示 logoPath 输入框，因为内置供应商的 logo 是固定的。

**布局**：
```
┌──────────────────────────┐
│      ┌──────────┐        │
│      │ 64x64    │        │
│      │ SVG Logo │        │
│      └──────────┘        │
│  内置供应商：固定显示       │
│  自定义供应商：点击弹出文件选择 │
│                           │
│  供应商名称: [________]    │
│  网站 URL:   [________]    │
│  (按类型显示认证配置)       │
└──────────────────────────┘
```

### D6. 自定义供应商 Logo 点击选图
**为什么**：自定义供应商没有固定 logo，用户可通过点击顶端头像弹出 `file dialog` 选择本地图片。选择后保存到 `logoPath` 字段，与现有 displayProvider 的 logo 渲染通路兼容。内置供应商的 avatar 不可点击。

## Risks / Trade-offs

- [R1] CodexZH API 端点 / 响应格式未验证 → 子 agent 基于推测实现，集成后需用真实 API Key 测试验证
- [R2] KWallet 首次使用时需要用户授权弹窗 → 与 MiniMax 行为一致，在 saveCredential() 时触发
- [R3] 周限额数据格式与现有 displayProvider 的 `usedPercent` 计算兼容性 → parser 返回的 used/total 直接映射到 QVariantMap，displayProvider 自动计算百分比

## Migration Plan

1. 从 stash@{1} 提取 codexzhclient.cpp/h、codexzhresponseparser.cpp/h
2. 修改 CMakeLists.txt 注册 4 个源文件
3. 修改 aiusagewatcherapplet.cpp/h 添加 CodexZhClient 桥接
4. 修改 main.qml 添加 CodexZH snapshot 接入
5. 修改 providerRegistry.js 修正为周限额
6. 改造 ProviderEditor.qml：顶端 64x64 圆形 Logo 头像（内置固定 / 自定义可点击选图），移除 logoPath 输入框
7. 改造 ProvidersConfig.qml：配置列表项 Logo 缩略图保持一致
6. 编译验证（`cmake --build build`）
7. 安装测试（`kpackagetool6 --upgrade package`）

## Open Questions
```

Full source: openspec/changes/codexzh-cpp-integration/design.md

## openspec/changes/codexzh-cpp-integration/tasks.md

- Source: openspec/changes/codexzh-cpp-integration/tasks.md
- Lines: 1-34
- SHA256: 5fa60540a038b8826eb7b0be4ddab358e6edcd446d6160fff7cbcf5694c87e85

```md
## 1. 提取子 agent C++ 代码并注册构建

- [ ] 1.1 从 stash@{1} 提取 codexzhclient.cpp/h、codexzhresponseparser.cpp/h 到 src/ 目录
- [ ] 1.2 修改 CMakeLists.txt 注册 4 个新源文件 + 添加 test 目标
- [ ] 1.3 编译验证（`cmake --build build`），修复任何编译错误

## 2. applet 桥接 CodexZhClient

- [ ] 2.1 在 aiusagewatcherapplet.h 添加 Q_PROPERTY（codexzhSnapshot/codexzhLoading/credentialConfigured/credentialStatus/credentialBusy/credentialError）
- [ ] 2.2 在 aiusagewatcherapplet.cpp 添加 CodexZhClient 成员初始化、signal 连接、getter 方法
- [ ] 2.3 添加 Q_INVOKABLE 方法：refreshCodexZhUsage/saveCodexZhApiKey/clearCodexZhApiKey
- [ ] 2.4 编译验证（`cmake --build build`）

## 3. QML 侧接入 CodexZH snapshot

- [ ] 3.1 main.qml 添加 codexzhSnapshot 读取 + applyCodexZhSnapshot() 函数
- [ ] 3.2 main.qml 添加 requestCodexZhRefresh() + Connections 监听 codexzhSnapshotChanged
- [ ] 3.3 main.qml refresh() 中串联 CodexZH 的刷新和 snapshot 应用

## 4. 配置表单 Logo 头像改造

- [ ] 4.1 ProviderEditor.qml 顶端添加 64x64 圆形头像区域：内置供应商显示固定内联 SVG（readOnly），自定义供应商点击弹出文件选择对话框
- [ ] 4.2 移除 ProviderEditor.qml 现有的 `logoPath` TextField 输入框
- [ ] 4.3 ProvidersConfig.qml 列表项 Logo 缩略图对齐新布局

## 5. providerRegistry.js 修正为周限额

- [ ] 5.1 将 codexzh plans 从 `[{ id: "daily", ... }, { id: "monthly", ... }]` 改为 `[{ id: "weekly", planName: "周限额", unit: "%" }]`
- [ ] 5.2 验证 resetPeriodSec 与周限额是否匹配（7*24*3600 或 30*24*3600）

## 6. 编译验证与集成测试

- [ ] 6.1 `cmake --build build` 编译通过
- [ ] 6.2 `kpackagetool6 --upgrade package` 安装通过，无 QML 运行时错误
- [ ] 6.3 确认 CodexZH 供应商在配置页可添加、Logo 正确显示、面板显示"未配置"状态```

## openspec/changes/codexzh-cpp-integration/specs/codexzh-native-client/spec.md

- Source: openspec/changes/codexzh-cpp-integration/specs/codexzh-native-client/spec.md
- Lines: 1-56
- SHA256: aa65f7cc1de42ac91b69535b39fdb71fd4928f11a081cfc056db48295f1df997

```md
## ADDED Requirements

### Requirement: CodexZH API Key 凭证安全存储

系统 MUST 使用 KWallet 安全存储 CodexZH 的 API Key，与 MiniMax 共享同一钱包文件夹（`AI Usage Watcher`），entry 名为 `CodexZH API Key`。

#### Scenario: 保存 API Key 到 KWallet
- **WHEN** 用户在配置页输入 CodexZH API Key 并保存
- **THEN** KWallet 弹出授权确认（首次），API Key 加密存储后 `credentialConfigured` 变为 true，`credentialStatus` 更新为"已配置"

#### Scenario: 清除 API Key
- **WHEN** 用户点击清除 CodexZH 凭证
- **THEN** KWallet 中对应 entry 被删除，`credentialConfigured` 变为 false，`credentialStatus` 更新为"未配置"

#### Scenario: 重启后自动加载凭证
- **WHEN** 小部件重启或 Plasma 重启
- **THEN** 自动打开 KWallet 读取已保存的 API Key，`credentialConfigured` 反映真实状态

### Requirement: CodexZH 周限额 HTTP 查询

系统 MUST 通过 HTTP API 查询 CodexZH 的周限额数据，API Key 通过请求头鉴权。

#### Scenario: 成功查询周限额
- **WHEN** `refreshCodexZhUsage()` 被调用且 API Key 已配置
- **THEN** 发起 HTTP GET 请求到 CodexZH API，解析 JSON 响应，返回的 snapshot 包含 `{ planId: "weekly", planName: "周限额", used, total, unit: "USD" }`，`loading` 状态正确切换

#### Scenario: API Key 无效
- **WHEN** API Key 无效或过期
- **THEN** 返回 HTTP 401/403，`credentialError` 变为 true，`errorText` 显示"CodexZH 认证失败，请检查 API Key"

#### Scenario: 网络异常
- **WHEN** 网络不可用或请求超时
- **THEN** `errorText` 显示对应错误信息，`loading` 恢复 false，不抛出异常

### Requirement: QML 面板展示 CodexZH 数据

系统 MUST 在 QML 面板中展示 CodexZH 的周限额数据，与现有供应商（MiniMax/Codex）一致的 UI 形式。

#### Scenario: 正常显示周限额进度条
- **WHEN** 成功查询到周限额数据且 `codexzhSnapshot` 已更新
- **THEN** ProviderGroup 显示"CodexZH"标题、周限额进度条（used/total/百分比）、logo 和状态标签

#### Scenario: 未配置时显示占位
- **WHEN** 未保存 API Key
- **THEN** ProviderGroup 显示 statusLabel="凭证未配置"、无进度条数据

### Requirement: providerRegistry 注册 CodexZH 周限额

系统 MUST 在 providerRegistry.js 中将 CodexZH 注册为周限额供应商。

#### Scenario: 预设配置正确
- **WHEN** 用户从配置页添加 CodexZH 供应商
- **THEN** catalogId="codexzh" 的 plans 包含 `{ id: "weekly", planName: "周限额", unit: "%" }`，logoSvg/website/defaultLogoChar 正确

#### Scenario: 旧配置兼容
- **WHEN** 用户有旧配置（包含 "daily"/"monthly" plan 的 codexzh）
- **THEN** 新代码加载后替换为单周限额计划，不崩溃```

