# Verify Report: minimax-cc-switch-parity

日期：2026-08-04 ｜ 验证模式：full ｜ 流程：Comet hotfix

## 验证证据

| 检查项 | 结果 |
|--------|------|
| C++ 编译（生产插件目标） | PASS（guard build_command） |
| ctest 全量 | **9/9 PASS**（含新增 codexzh-response-parser） |
| QML 测试 qmltestrunner | **126/126 PASS**（fresh，4.1s） |
| tests/run-static-checks.sh | PASS |
| 安装级冒烟 run-plasma-smoke.sh | PASS（dlopen 插件加载、tst_fullView 16 项） |
| 部署实机验证 | PASS（plasmoid 安装 + systemd 重启 + 日志无 QML 错误） |
| 凭据保护：错误消息不含 Key；请求结束 `m_activeApiKey.fill('\0')` | PASS（finishRefresh:254-256） |

## 三维度核对

### Completeness
- tasks.md：19/19 勾选 ✓
- spec requirements（3 条）实现映射：
  1. MiniMax Coding Plan 接口（2 端点/请求头/SameOriginRedirectPolicy）→ `endpointCandidates` + `createRequest`，测试锁定 ✓
  2. MiniMax 响应解析（8 条规则）→ 全部实现，13 个解析器测试覆盖 ✓
  3. MiniMax 凭据保护 → 错误消息不含凭据 + 活跃 Key 即时清零 ✓

### Correctness
- Scenarios 覆盖：端点候选 ✓（测试）、凭据失败 ✓（`doesNotFallbackAcrossRegionsOnAuthenticationFailure` 本会话补齐）、完整响应 ✓、缺字段 ✓（5 个新测试）、仅 video ✓、周 status 非 1 ✓
- **WARNING**：网络/服务瞬时错误跨区域场景无自动化测试（实现存在 `requestNextEndpoint` 递归，仅凭代码审查确认）

### Coherence
- design.md 决策对照：401/403 分支 ✓、错误消息文案 ✓、解析规则表 ✓（本会话将"无 model_remains"口径与 spec/design 对齐）、跳过语义 ✓
- **WARNING**：design.md §3.2 测试名 `treatsGeneralAsFiveHourAndWeekly` 与实现 `parsesPercentageResponse` 不一致（文档名漂移，覆盖无缺）
- **WARNING**：design.md 验收 3"git diff 仅触及 src/minimax* 等"——commit 1 的 minimaxclient.cpp 内含钱包重试上限与旧文件夹迁移（同文件不可分；用户决策批准，见审核修复轮）。接受原因：文件级提交约束 + 用户明确选择保留改名+迁移
- **SUGGESTION**：C2 语义（base_resp 缺 status_code → api_error）最初在暂存区与工作区漂移，本会话已统一并补测试

## 分支处理

- 改动直接在 master 提交（7 个提交：75c8f6d..515f261），无独立 feature 分支
- 用户指示：全部提交 master → 已推送 gitee origin（`a51fa80..515f261`）
- gitee 仓库显示名已改 `AIQuotaPilot`（API PATCH；URL path 保持 `aiUsageWatcher`，gitee API 不支持 path 改名，网页端可改）
- 未提交项（非本 change）：并行会话的 opencodego client 半成品（编译错误）、docs/superpowers/plans/ 草稿、构建产物目录

## 结论

无 CRITICAL。3 个 WARNING/SUGGESTION 已记录接受原因（文档名漂移、测试名差异、验收范围因用户决策扩展）。**Ready for archive。**
