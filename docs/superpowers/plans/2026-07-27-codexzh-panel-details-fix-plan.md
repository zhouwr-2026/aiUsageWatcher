# CodexZH Panel Details Fix Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修正 Logo 底色、CodexZH 周套餐文案与重置时间，并为截断详情增加完整提示。

**Architecture:** 复用现有 `resetText` 和 Plasma ToolTip；解析器只输出展示模型需要的数据。周重置按本地自然周计算为下周一 00:00，客户端继续负责把计划映射为 QVariant。

**Tech Stack:** C++/Qt 6、QML/Kirigami、Qt Test

## Global Constraints

- 不修改供应商配置、凭据、请求或刷新逻辑。
- 不新增依赖。
- 周重置按自然周固定为下周一 00:00。

---

### Task 1: CodexZH 周计划数据

**Files:**
- Modify: `src/codexzhresponseparser.h`
- Modify: `src/codexzhresponseparser.cpp`
- Modify: `src/codexzhclient.cpp`
- Test: `tests/cpp/tst_codexzhresponseparser.cpp`

**Interfaces:**
- Consumes: 本地当前时间
- Produces: `CodexZhPlan::resetText`，套餐名“周限额”

- [ ] **Step 1: 先补失败断言**

固定当前时间为 `2026-07-27 14:00:00`，断言套餐名为“周限额”、重置时间为 `08-03 00:00`。

- [ ] **Step 2: 运行定向测试并确认失败**

Run: `cmake --build build --target tst_codexzhresponseparser && ctest --test-dir build -R codexzh-response-parser --output-on-failure`

Expected: FAIL，当前仍为 `Usage Stats` 且重置时间为空。

- [ ] **Step 3: 最小实现**

按本地时间计算下一次周一 00:00；把结果经 `CodexZhPlan::resetText` 传给现有 QVariant 字段。

- [ ] **Step 4: 重跑定向测试**

Expected: PASS。

### Task 2: Logo 与完整详情提示

**Files:**
- Modify: `package/contents/ui/ProviderGroup.qml`
- Modify: `package/contents/ui/PlanBar.qml`
- Test: `tests/tst_providerGroup.qml`

**Interfaces:**
- Consumes: 现有 `providerName`、`extraText`
- Produces: Codex 专用白底；详情 Label 的原生悬停 ToolTip

- [ ] **Step 1: 调整 Logo 容器**

非 Codex Logo 容器透明且无通用外圈；Codex 保留白色圆底。

- [ ] **Step 2: 增加原生 ToolTip**

详情 Label 继续 `Text.ElideRight`，使用 `HoverHandler` 控制 `PlasmaComponents.ToolTip`，内容为未截断的完整详情字符串。

- [ ] **Step 3: 运行 QML 定向测试**

Run: `qmltestrunner -input tests/tst_providerGroup.qml`

Expected: PASS。

### Task 3: 部署验证

**Files:** 无新增文件。

- [ ] **Step 1: 构建与回归**

Run: `cmake --build build && ctest --test-dir build --output-on-failure`

Expected: PASS。

- [ ] **Step 2: 安装并重启正式面板**

Run: `cmake --install build && systemctl --user restart plasma-plasmashell.service`

Expected: plasmashell 为 `active`。

- [ ] **Step 3: 正式界面验证**

确认 Logo、周限额、重置时间和详情 ToolTip 均符合 spec。
