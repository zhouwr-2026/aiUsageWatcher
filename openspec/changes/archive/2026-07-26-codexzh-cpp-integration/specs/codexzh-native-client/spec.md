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
- **THEN** 新代码加载后替换为单周限额计划，不崩溃