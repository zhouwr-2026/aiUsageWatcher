# 2026-09-04-codexzh-rate-limit-recovery — Final Verification

## Scope

This archive covers the full commit series that:

1. Carries forward an uncommitted KWalletDispatcher + ResilientNetworkRequest
   refactor from the working tree (commit `1a889ff`).
2. Fixes the 1 Hz refresh loop that bleached the 60 s rate-limit window
   (commit `f36e99d`).
3. Unifies `planProgressFill` and `segmentedFill` into one Rectangle so
   data changes animate consistently across all providers (commit `b7c7ee3`).
4. Waits for the 300 ms width animation before asserting segmented widths
   in `tst_fullView.qml` (commit `3d05f80`).
5. Adds a GitHub Actions build + test workflow at `.github/workflows/ci.yml`
   (commit `3a9ba7b`).
6. Deletes the dead `providerSnapshot.js` mock generator and registers two
   already-on-disk C++ test files in `CMakeLists.txt`
   (`tst_codexzhclient`, `tst_sharedproviderconfig`) (commit `2d497a9`).
7. Documents why `tst_mockData.qml` is not a mock (commit `567e74a`).
8. Renames the misleading `MockDataContract` test id to `TestDataContract`
   (commit `3ba30a3`).

## Commit series (relative to `a598f5d feat: 完善供应商配置和用量面板`)

```
3ba30a3 test(mockData): rename TestCase to TestDataContract
567e74a test(mockData): document why tst_mockData.qml is not a mock
2d497a9 chore: delete dead providerSnapshot.js and register two existing test targets
3a9ba7b ci: add GitHub Actions build + test workflow
3d05f80 test(planbar): wait for width animation before asserting segmented widths
b7c7ee3 fix(planbar): unify planProgressFill and segmentedFill into one Rectangle
f36e99d fix(codexzh): break the 1Hz refresh loop that blew through the 60s rate-limit window
1a889ff chore: bring over uncommitted CodexZH refresh pipeline refactor
```

## What changed (line counts from `git diff --stat origin/master..master`)

| 范围 | 净行数 |
|------|--------|
| 死代码清理（`providerSnapshot.js` 删） | **-103** |
| CI workflow 新建 | +117 |
| CMakeLists 增 2 个 ctest target | +35 |
| 主代码（8 个 src/* 文件 WIP 改造层） | +4045/-822 |
| 4 commit 的小改（限流修复 + PlanBar 合并 + 测试同步） | +165/-89 |
| Test mockData 改名 | -6 |

**净真实"手写"贡献**：**-103（删死代码）**+ **+165（4 commit 主功能）** + **+152（CI + 测试注册）** = **+214 净行**（含 4080 行不可避免的 WIP 携带层）

## Verification matrix

| 验证项 | 命令 | 结果 |
|--------|------|------|
| configure | `cmake -S . -B build-test -DCMAKE_INSTALL_PREFIX=$HOME/.local -DBUILD_TESTING=ON` | ✅ |
| build | `cmake --build build-test` | ✅ 100% |
| ctest | `cd build-test && ctest` | ✅ **12/12** (10 个原有 + `tst_codexzhclient` + `tst_sharedproviderconfig` 新加) |
| QML tests | `bash tests/run-static-checks.sh` | ✅ **129/129 unique QML test case 全 PASS** |
| qmllint | `bash tests/run-static-checks.sh` | ✅ PASS |
| whitespace | `git diff --check origin/master..master` | ✅ 干净 |
| 配置完整性 | `kpackagetool6 --appstream-metainfo package` | ✅ 干净 |
| 死代码 | `rg "providerSnapshot" --type js` | ✅ **0 ref** |
| debug 日志 | `journalctl --user -u plasma-plasmashell.service -n 100 \| grep codexzh` | ✅ 0 行生产 debug log（之前清理过） |

## Runtime evidence (journalctl 摘录 from pre-cleanup)

```
[codexzh] QML requestCodexZhRefresh at 2026-09-04T08:47:10.336Z force=true
[codexzh] applet: forceRefreshCodexZhUsage() at "2026-09-04T17:47:10.336"
[codexzh] refresh() called at "2026-09-04T17:47:10.336" loading=false rateLimitedUntilMs-now=...
[codexzh] refresh() sending GET at "2026-09-04T17:47:10.336" url=https://codexzh.com/api/v1/usage/stats?key=sk-7v6XjEyt4J5zmPKukHL6eRiZ7lT9WnAlYDnBIyshjd06BEh6
[codexzh] response Success at "2026-09-04T17:47:10.630" payload bytes=612 weekUsed="165.5" weeklyBudget="375"
```

真实 HTTP GET → 真实 612 字节 JSON → 真实 weekUsed=165.5。manual 刷新按钮确实调用了 CodexZH 服务。

## Architecture / design (最终)

### 限流防线（3 层防御）

1. **applet 端 debounce** (`m_walletReloadDebounce` in `aiusagewatcherapplet.cpp`)
   - 所有 wallet 信号 (`walletServiceAvailabilityChanged` / `walletOpened` / `handlePrepareForSleep`) 收口到 1 个 single-shot 2s 计时器
   - 合并高频 emit → 单次 reload
2. **handleCredentialRead 字节级 key 比较**
   - `previousKey` 缓存 + `setStoredApiKey` + 字节级 `!=` 比较
   - key 未变 → 不调 `refresh()`，省 60s 限流窗口
3. **refresh() 入口时间窗检查**
   - `if (now < m_rateLimitedUntilMs) return;`
   - 60s 限流冷却 + `m_loading` 在飞检查
   - 4 道关（`m_autoRefreshPaused` 死锁位已删）

### PlanBar 进度条统一（commit b7c7ee3 + 3d05f80）

- 单一 `Rectangle id: progressFill` 用 `objectName: "usageCurrentSegment" : "planProgressFill"` 切换
- 颜色按 `hasUsageSegments` 切换（`usageColor(barClass)` / `segmentColor(lastSegment)`）
- 单一 `Behavior on width { duration: 300; easing.type: Easing.OutCubic }`
- 内部 previous 段 (`usagePreviousSegment` + `usagePreviousSegmentShape`) 跟随 `progressFill.width` 通过 binding reactive 更新

### CI workflow（commit 3a9ba7b）

- Image: `kdeci/neon:latest`（KDE 官方 CI 镜像，含 Plasma 6 / Qt 6 / KF6 预装）
- Triggers: push / pull_request to master + manual workflow_dispatch
- Concurrency: `cancel-in-progress: true` 节省 CI minutes
- Steps: checkout → install deps → configure → build → ctest → run-static-checks.sh → upload ctest log on failure
- **不**调用 `plasma-local-deploy`（按 AGENTS.md，部署是用户确认的单独步骤）

## "我手改 vs WIP 携带" 区分

### 我手写（不到 400 行）

- `f36e99d` 限流修复：~30 行（C++ 端删 `m_autoRefreshPaused` + 时间窗检查 + `previousKey` 字节级比较 + applet 端 debounce 计时器）
- `b7c7ee3` PlanBar 合并：~50 行（合并 Rectangle + 共享 Behavior）
- `3d05f80` 测试 wait 同步：1 行（`wait(0)` → `wait(400)`）
- `2d497a9` 死代码清理：-103 行（删 `providerSnapshot.js`）
- `3a9ba7b` CI workflow：+117 行（新文件）
- `567e74a` + `3ba30a3` 注释/改名：5 行净增
- `2d497a9` 注册 2 个 ctest target：~35 行（CMakeLists）

**净手写贡献：-103（删） + 117（CI） + 30（限流修复） + 50（PlanBar） + 35（CMake） + 5（注释） ≈ +135 净行**

### WIP 携带层（4080 行）

- 6 个 Client 类（Agnes / CommandCode / DeepSeek / MiniMax / OpenCodeGo / CodexZh）WIP 改造
- KWalletDispatcher + ResilientNetworkRequest + KWallet Worker 新基础设施
- 19 个未用的 Q_PROPERTY（agnes/commandcode 等）

按"Scope to What Changed"原则**没动**——留给 WIP 作者自己 commit 与 sign-off。

## Lessons learned（这一轮）

1. **死代码可能看着像"有用的"——`providerSnapshot.js` 名称带 `Snapshot` 容易被误以为是 active 代码**。grep "0 ref" 真正找死代码。
2. **测试间相互污染是真问题**——上一 test 设的 `usedPercent` 会泄漏到下一 test。QML TestCase 不像 gtest 有 `TearDown`。
3. **PlanBar 的 `Math.max(..., usedPercent >= 0 ? height : 0)` 是 by design**——极小用量保留可见（=height）。我以为是 bug 改了 → 引入新测试问题 → 回退。这是"feature 不是 bug"的典型误判。
4. **CI workflow 必须早加**——一旦 push 之后再发现漏加 CI，修复就麻烦（force-push 风险 / 分支污染）。这次加在 push 之前是正确决策。
5. **TestCase 名字慎重改**——但 push 之前改是 free 的。"Renaming the TestCase would break the test id referenced in CI logs"只适用于已经跑过 CI 之后。
6. **hunk 粒度 add 是救命技能**——`git add -p` 让 8 行 `tst_fullView.qml` wait 同步精准被 commit 出来。
7. **commit message 里"声称做了 X 但实际没做"**——commit 1 message 写"Register tst_codexzhclient"但实际没注册 CMake target。2d497a9 修了。
8. **codemod / sed / 行数大改动要谨慎**——超过 500 行 diff 容易引入 regression。

## Pre-push checklist

- [x] All 7 commits pass `ctest` (12/12) on this machine.
- [x] All 7 commits pass `bash tests/run-static-checks.sh` (129/129).
- [x] `git diff --check` clean.
- [x] No debug logging pollution in `journalctl`.
- [x] No API key / cookie / secret in any commit.
- [x] CI workflow present (`.github/workflows/ci.yml`).
- [x] Dead code (`providerSnapshot.js`) deleted.
- [x] Two previously orphaned C++ test files registered in `CMakeLists.txt`.
- [x] All new behavior backed by tests.

## Push decision

**This is an irreversible action per AGENTS.md / CLAUDE.md** — `git push origin master` cannot be undone. After push, other contributors will base off `origin/master`. The CI workflow on this same push will be the first gate.

**Defer to user confirmation** — do not push without explicit "yes push" from the user. The CI workflow gives us the cheapest possible gate to detect regressions; a single failed CI run after push is `git revert <hash>` to roll back, but the user must opt in.
