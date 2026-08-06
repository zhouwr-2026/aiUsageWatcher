# MiniMax KWallet 命名修复热修 — Design

## 0. 根因复盘

**关键事实链**：

- 项目当前 KPlugin Id 是 `org.kde.plasma.AIQuotaPilot`，界面名是“额度领航员”。
- `src/minimaxclient.cpp` 和 `src/codexzhclient.cpp` 仍使用旧 KWallet folder `AI Usage Watcher`。
- 本机旧 folder 中存在 `MiniMax API Key` 与 `CodexZH API Key` entry。
- 当前 MiniMax 请求格式已经对齐 cc-switch，独立 curl / Qt 客户端在正确凭据下可成功。
- 面板仍 1004 的更强解释是：运行中的 applet 继续读取旧 folder 中的历史凭据。

**结论**：这不是 1004 限流重试问题，而是项目改名后凭据存储命名没有同步。

## 1. KWallet folder 命名

在 MiniMax 与 CodexZH 客户端中统一改为：

```cpp
const QString walletFolder = QStringLiteral("AIQuotaPilot");
```

选择 `AIQuotaPilot` 而不是“额度领航员”的原因：

- 与当前仓库名、CMake project、KPlugin Id 保持一致。
- 避免把可翻译 UI 名称用于不可见的持久化存储路径。
- ASCII folder 更适合命令行诊断。

## 2. 不做自动迁移

不从旧 folder `AI Usage Watcher` 自动读取或复制密码。

原因：

- 旧 folder 当前正是疑似错误凭据来源。
- 自动迁移会把 stale Key 带进新 folder，导致 1004 继续存在。
- 不读取旧密码可以满足“不读密钥、不输出密钥”的安全约束。

升级后的行为：

- 新安装或首次运行会创建/使用 `AIQuotaPilot` folder。
- 如果新 folder 无对应 entry，客户端显示未配置。
- 用户在配置页保存 API Key 后，写入新 folder。
- 旧 folder 保留不动。

## 3. MiniMax 请求逻辑

不再按旧设计添加 1004 跨区域重试。

理由：

- 该策略无法修复错误凭据导致的 1004。
- MiniMax 官方没有在本项目内可验证的 1004 限流契约。
- cc-switch 参考实现也没有特殊处理 1004。

保留现有行为：

- `base_resp.status_code != 0` 仍作为业务错误展示。
- 401/403 HTTP 状态仍早返回。
- endpoint candidates 仍为 cn / intl 两个 Coding Plan 端点。

## 4. 测试策略

本次改动只有两个持久化命名常量，现有单元测试不直接打开真实 KWallet。验证以三层为准：

1. 静态检查：`rg "AI Usage Watcher" src` 不再命中运行时代码。
2. C++ 测试：`ctest -R 'minimax|codex'` 通过，确认请求/parser 合约不被改坏。
3. 本机部署：安装后重新启动 plasmashell，保存 MiniMax Key 到新 folder，再刷新面板。

## 5. 验收

- `src/minimaxclient.cpp` 和 `src/codexzhclient.cpp` 不再引用 `AI Usage Watcher`。
- `kwallet-query -f "AIQuotaPilot" -l kdewallet` 能看到新保存的 entry 名称，但不读取密码。
- 面板刷新不再从旧 folder 取 Key。
- 不输出、不记录、不提交任何 API Key。
