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
- 需要 cmake 重新编译