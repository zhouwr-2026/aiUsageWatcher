# MiniMax cc-switch 兼容热修

## Why

MiniMax 固定供应商当前对 `/v1/token_plan/remains` 端点的依赖与本机 `cc-switch-main`
已切到的 `/v1/api/openplatform/coding_plan/remains` 不一致；同时解析层在响应字段缺失
或类型错配时会误判整份响应失败，导致悬浮面板出现"无法识别"误报，5 小时与每周额度展示
也被一并抹掉。

## What Changes

- `src/minimaxclient.cpp`：
  - 端点候选从 4 个收敛为 2 个（中国区 / 国际区 Coding Plan）。
  - HTTP 401/403 视为凭据无效，不再跨区域回退；其他网络错误允许跨区域。
- `src/minimaxresponseparser.cpp`：
  - 与 `cc-switch-main` 对齐：`model_remains[]` 中仅消费 `model_name == "general"`。
  - 5 小时已用 = `100 - current_interval_remaining_percent`（仅在字段是合法数字时）。
  - 周额度仅在 `current_weekly_status == 1` 且周剩余百分比是数字时展示。
  - `end_time` / `weekly_end_time` 缺失或类型不兼容时仅省略时间，不让整份响应失败。
  - 单个额度字段缺失或类型错配时跳过该项，保留其他项。
  - 合法响应但无 `general` 或无可展示额度时返回"未订阅"，不报"无法识别"。
- `tests/cpp/tst_minimaxclient.cpp`：锁定两个 Coding Plan 端点及请求头契约。
- `tests/cpp/tst_minimaxresponseparser.cpp`：补 cc-switch 主路径、字段缺失、非 general
  模型、周额度状态切换的测试。

## Impact

- 不影响其他固定供应商（Codex/GLM/Kimi 等）。
- 不涉及 API Key 存储、迁移或日志暴露；继续使用 `m_activeApiKey.fill('\0')` 模式。
- 不改变 KConfig schema、不改 QML；纯 C++/单元测试改动。