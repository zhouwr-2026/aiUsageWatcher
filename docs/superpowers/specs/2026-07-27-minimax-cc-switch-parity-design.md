# MiniMax cc-switch 兼容热修设计

## 目标

让 MiniMax 固定供应商使用与本机 `cc-switch-main` 一致的 Coding Plan 查询接口和响应语义，消除“MiniMax 返回了无法识别的数据”的误报，并恢复 5 小时、每周额度展示。

## 范围

- `src/minimaxclient.cpp`：仅请求中国区和国际区的 `/v1/api/openplatform/coding_plan/remains`，保留区域回退，不再混用 `/v1/token_plan/remains`。
- `src/minimaxresponseparser.cpp`：按 cc-switch 的 `general` 模型解析剩余百分比；重置时间缺失或类型不兼容时只省略时间，不判整份响应失败。
- `tests/cpp/tst_minimaxclient.cpp`：锁定两个 Coding Plan 端点及请求头。
- `tests/cpp/tst_minimaxresponseparser.cpp`：移植 cc-switch 主路径、缺失字段、非 general 模型和周额度状态测试。

## 数据规则

- 仅消费 `model_remains[]` 中 `model_name == "general"` 的对象。
- `current_interval_remaining_percent` 存在且为数字时，5 小时已用百分比为 `100 - remaining`。
- 仅当 `current_weekly_status == 1` 且周剩余百分比为数字时展示周额度。
- `end_time`、`weekly_end_time` 是可选展示信息；缺失、空值或非整数不影响额度值。
- JSON 无法解析、根对象无 `model_remains` 数组、`base_resp.status_code` 非零时才返回明确错误。
- 不记录响应正文、Authorization 头或 API Key；错误日志和用户提示不得包含凭据。

## 错误处理

- HTTP 401/403：提示 MiniMax Key 无效或已过期，不继续跨区域尝试。
- 网络错误、超时、5xx：允许尝试另一区域的 Coding Plan 端点。
- 合法响应但无 `general` 模型或无可展示额度：返回“未订阅”，不报“无法识别”。
- 单个额度字段缺失：跳过对应额度项，保留其他有效项。

## 验收

- MiniMax parser/client 定向测试全部通过。
- QML 快照转换与 CompactView 测试保持通过。
- 构建、安装后重载 Plasma Shell，配置有效 MiniMax Key 后，悬浮面板展示实际 5 小时额度；有周额度套餐同时展示每周额度。
- 不读取、输出或迁移用户 API Key。

## 后续阶段

本热修完成后，单独审查其余固定供应商的接口、鉴权和响应格式，产出兼容矩阵；配置页、悬浮面板视觉重构以及供应商多账号模型另立规格，不在本热修中混改。
