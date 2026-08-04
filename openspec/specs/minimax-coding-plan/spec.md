# minimax-coding-plan Specification

## Purpose
定义 MiniMax Coding Plan 用量查询的端点、请求头、凭据失败处理与响应解析语义（与 cc-switch 参考实现对齐）。
## Requirements
### Requirement: MiniMax Coding Plan 接口

小部件 SHALL 仅向 MiniMax Coding Plan 数据面查询用量：

- `GET https://api.minimaxi.com/v1/api/openplatform/coding_plan/remains`
- `GET https://api.minimax.io/v1/api/openplatform/coding_plan/remains`

请求头 SHALL 包含：`Authorization: Bearer <key>`、`Content-Type: application/json`、
`Accept: application/json`、`User-Agent: AIUsageWatcher/0.1`，并且不允许跨域重定向
（`SameOriginRedirectPolicy`）。

#### Scenario: 端点候选

- **WHEN** 客户端列举 MiniMax 端点候选
- **THEN** 返回恰好 2 个 URL，按中国区→国际区顺序，且均为 `/v1/api/openplatform/coding_plan/remains`

#### Scenario: 凭据失败

- **WHEN** 任一区域返回 HTTP 401 或 403
- **THEN** 客户端 SHALL 立即停止跨区域尝试，将错误消息设为"MiniMax Key 无效或已过期"，并返回 `ok=false` 的快照

#### Scenario: 网络/服务瞬时错误

- **WHEN** 端点返回网络错误、超时或 HTTP 5xx
- **THEN** 客户端 SHALL 自动尝试另一区域

### Requirement: MiniMax 响应解析

解析器 SHALL 与 `cc-switch-main` 的 `parse_minimax_tiers` 行为一致：

- 响应根对象须为 JSON 对象；根对象无 `model_remains` 数组时按未订阅处理（`ok=true`，`statusLabel="未订阅"`，无 plan）
- `base_resp.status_code` 必须存在且为 0；否则 `ok=false`，`errorCode=api_error`
- 数组中 SHALL 仅消费 `model_name == "general"` 的对象；其他模型对象忽略
- 5 小时已用百分比 = `100 - current_interval_remaining_percent`，仅在字段为合法数字时计算
- 周额度仅当 `current_weekly_status == 1` 且 `current_weekly_remaining_percent` 为合法数字时展示
- `end_time` / `weekly_end_time` 缺失或类型不兼容 SHALL 仅省略 `resetAtMs`，不影响额度值
- 单个额度字段缺失或类型错 SHALL 跳过该项，保留其他项
- 合法响应但无 `general` 模型或无可展示额度 SHALL 返回 `ok=true`、`statusLabel="未订阅"`

#### Scenario: 完整响应

- **WHEN** 响应含 `general` 对象且 5h/周字段均合法
- **THEN** 快照包含两条 plan：5 小时、每周

#### Scenario: 缺字段

- **WHEN** 响应含 `general` 但缺 `current_interval_remaining_percent`
- **THEN** 快照仅含周 plan（若周字段合法），整体 `ok=true`

#### Scenario: 仅 video 模型

- **WHEN** 响应 `model_remains` 中只有非 `general` 模型
- **THEN** 快照 `statusLabel="未订阅"`，`plans` 为空，`ok=true`

#### Scenario: 周 status 非 1

- **WHEN** `current_weekly_status != 1`
- **THEN** 快照不含周 plan

### Requirement: MiniMax 凭据保护

错误消息 SHALL NOT 包含 API Key 或 Authorization 头内容；活跃 Key 在请求结束
后立即调用 `fill('\0')` 清零。

#### Scenario: 错误消息不含凭据

- **WHEN** 任意 HTTP 失败（401/403/5xx/超时/网络）
- **THEN** 错误消息 SHALL 仅含可向用户展示的提示文案，不出现 API Key、Authorization 头或响应体截断片段

#### Scenario: 活跃 Key 内存清零

- **WHEN** `refresh()` 走完任意分支（成功 / 失败 / 凭据无效）
- **THEN** `m_activeApiKey` SHALL 被 `fill('\0')` 立即清零，且不再用于后续请求

