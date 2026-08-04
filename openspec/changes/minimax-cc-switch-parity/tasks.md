# MiniMax cc-switch 兼容热修 — Tasks

## Task 1: 收敛端点 + 凭据失败短路（src/minimaxclient.cpp）
- [x] `endpointCandidates` 由 4 端点减为 2 端点（Coding Plan 中国区 + 国际区）
- [x] 401/403 → `setError("MiniMax Key 无效或已过期")` 后 `finishRefresh`，不跨区域
- [x] 其他失败保持 `requestNextEndpoint` 跨区域语义

## Task 2: 解析器按 cc-switch 语义重写（src/minimaxresponseparser.cpp）
- [x] 5h 字段缺失或类型错：跳过该项（不再返回 false）
- [x] 周字段在 `current_weekly_status != 1` 或 `current_weekly_remaining_percent` 非数字时跳过
- [x] 重置时间字段缺失或类型错：tier 仍添加，`resetAtMs = 0`
- [x] 保留：JSON 解析失败、缺 `base_resp.status_code` / `status_code` 非零 → api_error；根对象无 model_remains 走未订阅
- [x] 保留：合法响应但无 general → `statusLabel="未订阅"`，`ok=true`

## Task 3: 客户端测试更新（tests/cpp/tst_minimaxclient.cpp）
- [x] `endpointCandidates` size 改为 2；首/末断言改为两个 Coding Plan 端点
- [x] 新增 `endpointCandidatesAreCodingPlanOnly` 锁定 size=2

## Task 4: 解析器测试更新（tests/cpp/tst_minimaxresponseparser.cpp）
- [x] 保留/调整现有 11 个 case 全绿（含 `rejectsInvalidPercentage` 与 `rejectsIntegerOutsideQint64Range` 的断言调整）
- [x] 新增 5 个 case：缺 5h 字段 / 缺周字段但 status=1 / 缺 weekly status / 周 status!=1 跳过 / 跳过 video 模型

## Task 5: 构建 + 测试
- [x] CMake 重新配置 + 构建
- [x] `ctest -R 'tst_minimax'` 全绿（18+6 PASS）
- [x] `tests/run-static-checks.sh` 通过（脚本自身 PASS；QML 测试失败为本次未触及的预先回归）
- [x] QML 快照测试的 minimax 相关用例未变动

## Task 6: 对抗性自审 + verify
- [x] `git diff --stat` 确认改动范围仅限 minimax 相关文件 + 本 change
- [x] 复核：不输出 API Key、不改其他供应商、不动 KConfig/QML
- [ ] 派独立 reviewer subagent（general-purpose）仅给 diff 与设计 doc，复核正确性与最小性