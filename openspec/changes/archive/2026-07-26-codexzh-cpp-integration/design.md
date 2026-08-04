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

- [ ] CodexZH API 实际端点 URL 和 JSON 响应结构需确认（子 agent 的实现需实测修正）
- [ ] 周限额的 resetPeriodSec 应该用 7 天还是 30 天？取决于 API 侧重置策略