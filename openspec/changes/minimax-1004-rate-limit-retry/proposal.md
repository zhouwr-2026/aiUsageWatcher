# MiniMax KWallet 命名修复热修

## Why

项目已改名为 AIQuotaPilot / 额度领航员，但 MiniMax 与 CodexZH 的 C++ 客户端仍把 API Key 存在旧 KWallet folder `AI Usage Watcher`。

这会造成两个问题：

- 配置与诊断时看到旧项目名，和当前产品命名不一致。
- MiniMax 刷新仍可能读取旧 folder 中的历史凭据，表现为接口返回 `base_resp.status_code = 1004`，即使同一台机器上用正确 `sk-cp-` Key 直连 Coding Plan 端点可以成功。

当前证据已经推翻“请求格式错误”和“单纯限流”两个方向：

- 当前 C++ 请求 URL 与 Header 已对齐 cc-switch 的 Coding Plan 请求。
- 独立 curl / Qt 请求在正确凭据下可以返回 `status_code=0`。
- 源码仍硬编码旧 folder，是当前仍 1004 的更强根因。

## What Changes

- 将固定供应商 API Key 的 KWallet folder 改为当前内部项目名 `AIQuotaPilot`。
- MiniMax 与 CodexZH 保持同一个 folder、不同 entry 的既有模型。
- 不自动读取、迁移或打印旧 folder 中的密码，避免把旧错误凭据静默带入新路径。
- 部署后，新配置保存会写入 `AIQuotaPilot` folder；旧 `AI Usage Watcher` folder 保留在钱包中，由用户按需清理。

## Impact

- MiniMax 刷新不再继续读取旧项目名 folder 中的历史 Key。
- CodexZH 也会跟随使用新 folder，保持项目命名一致。
- 已保存在旧 folder 的凭据不会被删除，也不会被自动复制。
- 用户需要在配置页重新保存一次 MiniMax / CodexZH API Key，之后面板使用新 folder。

## 风险与不做的事

**风险**：不自动迁移意味着旧用户第一次升级后会看到“未配置”或需要重新保存凭据。这比静默复制一个可能错误的旧 Key 更安全。

**不做**：

- 不读取、打印、哈希或记录任何 API Key。
- 不自动删除旧 KWallet folder 或 entry。
- 不继续实现 1004 跨区域重试；当前根因不是请求区域选择。
- 不改 MiniMax Coding Plan parser 语义。
