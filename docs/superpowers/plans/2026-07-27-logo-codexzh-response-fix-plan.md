# Logo and CodexZH Response Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Correct provider Logo backgrounds and map the CodexZH weekly budget response to absolute USD values.

**Architecture:** Keep the existing QML component and native parser. Change only their incorrect defaults/field mapping and protect both with focused tests.

**Tech Stack:** Qt 6, QML, Qt Test, CMake

## Global Constraints

- Do not log or expose API keys.
- Do not change persisted provider definitions or credential storage.
- Use the formal Plasma service for restart.

---

### Task 1: Logo background

**Files:**
- Modify: `package/contents/ui/ProviderGroup.qml`
- Test: `tests/tst_providerGroup.qml`

- [ ] Add a failing assertion that Codex uses an opaque white indicator background and MiniMax uses a transparent one.
- [ ] Change the existing indicator color expression from alternate background to transparent for non-Codex providers.
- [ ] Run `qmltestrunner -input tests/tst_providerGroup.qml -import package/contents/ui`; expect PASS.

### Task 2: CodexZH response mapping

**Files:**
- Modify: `src/codexzhresponseparser.cpp`
- Create: `tests/cpp/tst_codexzhresponseparser.cpp`
- Modify: `CMakeLists.txt`

- [ ] Add a failing parser test with `weeklyBudget=255`, `weekUsed=30.020896`, and `remainQuota=224.979104`.
- [ ] Read the real budget fields, use `quota / 500000` only as fallback, and store absolute `used` and `total` values.
- [ ] Align detail text with the real response field names.
- [ ] Build and run the focused C++ and QML tests; expect PASS.
- [ ] Install, restart `plasma-plasmashell.service`, and verify it remains active.
