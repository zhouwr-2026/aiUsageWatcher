# MiniMax cc-switch 兼容热修 — Design

参考实现：`/home/zhouwr/Project/CodeWorkspace/cc-switch-main/src-tauri/src/services/coding_plan.rs`
的 `query_minimax` 与 `parse_minimax_tiers`。

## 1. 客户端改动（`src/minimaxclient.cpp`）

### 1.1 端点候选

```cpp
QList<QUrl> MiniMaxClient::endpointCandidates()
{
    return {
        QUrl(QStringLiteral("https://api.minimaxi.com/v1/api/openplatform/coding_plan/remains")),
        QUrl(QStringLiteral("https://api.minimax.io/v1/api/openplatform/coding_plan/remains")),
    };
}
```

- 顺序：先中国区（默认用户），失败再国际区。
- 不再混用 `/v1/token_plan/remains`（老路径已被 cc-switch 弃用）。

### 1.2 401/403 不跨区域

在 `requestNextEndpoint` 的错误分支里增加：

```cpp
const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
const bool authFailed = (httpStatus == 401 || httpStatus == 403);
if (authFailed) {
    m_lastRequestError = QStringLiteral("MiniMax Key 无效或已过期");
    // 不调用 requestNextEndpoint，直接 finishRefresh 路径
}
```

实现方式：在 lambda 末尾区分两条分支 —— 凭据失败时设 snapshot(`errorText` 携带
"Key 无效或已过期"），并调用 `finishRefresh()`；网络/5xx/超时等瞬时错误仍走
`requestNextEndpoint` 跨区域。

### 1.3 错误消息（与设计文档对齐）

- 401/403 → "MiniMax Key 无效或已过期"
- 5xx / 网络 / 超时 → 允许跨区域；最终 fallback = "无法连接 MiniMax 服务"

## 2. 解析器改动（`src/minimaxresponseparser.cpp`）

### 2.1 与 cc-switch 对齐的解析规则

| 条件 | 行为 |
|------|------|
| JSON 解析失败 | `ok=false`，`errorCode=invalid_response` |
| 根对象无 `model_remains` 数组 | `ok=true`，`statusLabel="未订阅"`，无 plan |
| `base_resp.status_code` 非零 | `ok=false`，`errorCode=api_error` |
| `model_remains[]` 全部不是 `general` | `ok=true`，`statusLabel="未订阅"`，无 plan |
| 找到 `general` 但 5h/周字段都缺失 | `ok=true`，`statusLabel="未订阅"`，无 plan |
| 5h 字段 `current_interval_remaining_percent` 是数字 | 计算 `100 - v` |
| 5h 字段缺失或非数字 | 跳过 5h tier |
| 周字段需 `current_weekly_status == 1` 且 `current_weekly_remaining_percent` 是数字 | 计算 `100 - v` |
| 否则周字段 | 跳过周 tier |
| `end_time` / `weekly_end_time` 缺失或类型错 | tier 仍添加，`resetAtMs=0` |

### 2.2 关键修复

`appendIntervalQuota` / `appendWeeklyQuota` 在字段缺失或类型错时改为 **跳过该项而不
是让整份响应失败**。删除它们返回 `false` 的语义；改为 `bool` 表示"是否成功添加"，但
失败时不级联到 `invalidResponse()`。

## 3. 测试改动

### 3.1 `tests/cpp/tst_minimaxclient.cpp`

- `endpointCandidates` size 由 4 改为 2；首/末断言改为两个 Coding Plan 端点。
- 新增 `doesNotFallbackAcrossRegionsOnAuthenticationFailure`：构造两条响应
  （第一条 401、第二条 200），断言只消费第一条、错误信息含"Key 无效或已过期"。

### 3.2 `tests/cpp/tst_minimaxresponseparser.cpp`

保留现有 11 个测试；补：
- `treatsGeneralAsFiveHourAndWeekly`（来自 cc-switch 主路径）
- `skipsVideoModelEvenIfFirstItem`
- `acceptsGeneralWithMissingIntervalPercent`
- `acceptsGeneralWithMissingWeeklyPercentButStatusOne`
- `acceptsGeneralWithMissingWeeklyStatus`
- `skipsWeeklyWhenStatusNotOne`

## 4. 验收

1. `ctest -R 'tst_minimax(client|responseparser)'` 全绿。
2. QML 测试不回归（`tests/tst_compactView.qml`、`tst_providerNormalize.qml` 等）。
3. `git diff` 仅触及 `src/minimax*`、`tests/cpp/tst_minimax*` 与本 change 内的 proposal/design/tasks。
4. 不读取、输出或迁移用户 API Key；错误消息不含凭据。