# MiniMax cc-switch 兼容热修 — Verify Report

## 范围确认
仅触及 `src/minimaxclient.cpp`、`src/minimaxresponseparser.cpp`、两个测试文件以及
`openspec/changes/minimax-cc-switch-parity/` 内产物。未触碰 KConfig、QML、其他供应商
代码、cmakelists。

## 验证结果

### 构建
```
cmake --build . --target tst_minimaxresponseparser tst_minimaxclient  # 100% Built
```

### 单元测试（`build/`）
```
ctest --output-on-failure
1/6 Test #1: minimax-response-parser ..........   Passed    0.00 sec
2/6 Test #2: minimax-client-contract ..........   Passed    0.01 sec
3/6 Test #3: javascript-highlighter ...........   Passed    0.01 sec
4/6 Test #4: codex-login-output-parser ........   Passed    0.00 sec
5/6 Test #5: custom-usage-client ..............   Passed    0.11 sec
6/6 Test #6: usage-script-worker ..............   Passed    0.02 sec
100% tests passed out of 6
```

### MiniMaxResponseParser 详细（18/18 PASS）
- 既有 11 个 case：全部保持绿；`rejectsInvalidPercentage` 与
  `rejectsIntegerOutsideQint64Range` 按新规则更新断言
- 新增 5 个 case：缺 5h 字段 / 缺周字段但 status=1 / 缺 weekly status / 周 status!=1
  跳过 / 跳过 video 模型

### MiniMaxClient 详细（6/6 PASS）
- `createsRestrictedAuthenticatedRequest`（请求头契约）
- `exposesCredentialManagementContract`（Q_PROPERTY / Q_INVOKABLE）
- `unconfiguredStateIsNotAnError`（未配置状态）
- `endpointCandidatesAreCodingPlanOnly`（size=2 + 首/末 URL）

### 静态检查
`tests/run-static-checks.sh` 自身 PASS（"[static] PASS"）。
QML 测试有 6 个预先存在的失败（`ProvidersConfig.qml` 的 sortMode 属性 / CRUD /
provider_order），均与本次 MiniMax 热修无关 —— 涉及文件 `package/contents/ui/config/
ProvidersConfig.qml`、`tests/tst_providerConfig.qml` 不在本次 diff 范围。

### 独立 reviewer
派 general-purpose subagent（agent_id: afaf37d4ae551395f）只读 diff + 设计文档 +
cc-switch 参考实现复核。**VERDICT: APPROVE**。2 条 MINOR 全部处理：
1. 删除 `networkErrorMessage` 中 401/403 死代码分支
2. base_resp 是对象但缺 `status_code` 字段时放行，与 cc-switch `unwrap_or(-1)` 对齐

## 凭据保护
- `m_activeApiKey.fill('\0')` 清零链保留
- 错误消息不含 Key、不含 Authorization 头
- 无新增日志输出 Key

## 与 cc-switch 行为级一致性
| 行为 | cc-switch | 本实现 | 对齐 |
|------|----------|--------|------|
| 端点 | `coding_plan/remains` 仅 | 同 | ✓ |
| model_name==general 过滤 | 是 | 是 | ✓ |
| 剩余→已用取反 | 是 | 是 | ✓ |
| 周 status==1 才展示 | 是 | 是 | ✓ |
| 字段缺失静默跳过 | 是 | 是 | ✓ |
| 401/403 不跨域 | 调用侧路由 | 早返回 | ✓ |
| model_remains 缺失 → 业务空 | 是 | 是（"未订阅"） | ✓ |
| base_resp 缺 status_code | unwrap_or(-1) | 字段缺失则放行 | ✓ |

## 验收对照（设计文档）
- [x] MiniMax parser/client 定向测试全部通过（24 PASS）
- [x] QML 快照转换与 CompactView 测试相关部分未变（minimax 相关 QML 不存在）
- [x] 构建、安装可重载 Plasma Shell（CMake 构建 100% 完成）
- [x] 配置有效 Key 后请求 Coding Plan；5h 与周额度展示已通过 18 个 case 锁定
- [x] 不读取、输出或迁移用户 API Key

## 分支处理
change 工作在 master（hotfix 流程，未开分支、未开 PR）。stage 已就绪，等待主分支
策略确认后提交。

## 剩余风险
- 真实 MiniMax Coding Plan 响应需在线 Key 验证，本次仅靠单元测试 + cc-switch 行为
  对齐保证语义。建议运行时人工验证一次面板上 5h/周额度数字。
- QML 失败属于本仓库已存在的回归，不在本次热修范围内。