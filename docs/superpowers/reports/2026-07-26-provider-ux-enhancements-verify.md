## Verification Report: provider-ux-enhancements

### Summary

| Dimension    | Status           |
|--------------|------------------|
| Completeness | 11/11 tasks ✅   |
| Correctness  | 7/7 reqs covered (1 warning) |
| Coherence    | Design decisions followed ✅ |

### Issues by Priority

#### CRITICAL (Must fix before archive)

**C1: 3 处旧 `MockData.buildDisplayProviders` 调用未迁移**
- **文件**: `package/contents/ui/main.qml:67,87,109`
- **问题**: `applyMiniMaxSnapshot()`、`applyCodexSnapshot()`、`applyCustomSnapshots()` 三函数仍使用 `MockData.buildDisplayProviders()` 而不是新的 `DisplayProvider.buildDisplay()`，导致通过 C++ backend 推送的 snapshot 数据不会经过排序、过滤等新功能层。
- **影响范围**: 当 C++ backend 调用 MiniMax/Codex/custom 的 snapshot setter 时，providers 列表按旧路径组装，不包含 sort、filter、logo 等新属性。面板显示可能跳回未排序状态。
- **推荐修复**: 将三函数中的 `MockData.buildDisplayProviders(providerDefinitions, runtimeSnapshots)` 替换为带 sortMode/customOrderRaw 参数的 `DisplayProvider.buildDisplay()`。

**C2: dirty diff 含 .comet.yaml 和 providerRegistry.js 的残留修改**
- **文件**: `openspec/changes/provider-ux-enhancements/.comet.yaml`（phase/verify_mode）、`package/contents/js/providerRegistry.js`（Claude Code URL + CodexZH logo）
- **问题**: 上次 session 的修改未提交。.comet.yaml 的 `phase: verify` 和 `verify_mode: full` 是 verify 阶段的预期状态；providerRegistry.js 的 Claude Code URL 更新和 CodexZH logo 变更来自 CodexZH 子 agent 的残留。
- **处理**: 提交 .comet.yaml 的 verify 阶段变更；providerRegistry.js 的 Claude Code URL 更新（`anthropic.com`→`claude.com`）是正确的 catalog 修复，应纳入；CodexZH logo 变更应随 stash 保留给独立 change。

#### WARNING (Should fix)

**W1: Req 6 仍有 mock 数据路径**
- **问题**: `createSeedSnapshots` 在 `providerDefinitionsChanged` 和 `refresh()` 时调用生成初始 seed。这些 seed 只用于 extractor 未就绪时的占位（显示"未配置"状态），不生成随机浮动数字，符合 spec "不使用随机/mock 数据"的要求。但 `SEED_RUNTIME_SNAPSHOTS` 在 `mockData.js` 中定义，建议迁移到 `providerSnapshot.js`。
- **推荐**: 将 `createSeedSnapshots` 和 `SEED_RUNTIME_SNAPSHOTS` 从 `mockData.js` 迁移到 `providerSnapshot.js`，逐步淘汰 mockData.js。

**W2: Orb 动画总时长超 plan 上限**
- **文件**: `CompactView.qml`
- **问题**: tasks.md 记录 final-branch-review 确认的 Minor 问题——动画总时长 240ms 略超 plan 200ms 上限。由 scale(120ms) + opacity(120ms) 组成，非关键路径问题。
- **状态**: 用户已知，已记录在 tasks.md。

### Final Assessment

**2 CRITICAL issues found. Fix before archiving.**

关键缺陷（C1）是 3 处旧 API 调用未迁移，导致通过 C++ backend 推送的数据绕过新功能层。修复方式直接替换为新的 `DisplayProvider.buildDisplay()` 调用。C2 是 dirty diff 处理：提交应属当前 change 的变更，保留 CodexZH logo 给独立 change。

W1/W2 属于改进建议和已知 Minor 问题，不影响功能正确性。

建议修复 C1 后进入分支处理，然后归档。