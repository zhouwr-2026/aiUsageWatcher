---
change: minimal-viable-plasmoid
design-doc: docs/superpowers/specs/2026-07-21-kcm-and-refresh-button-design.md
base-ref: ebbf3df2892413a10c64a2637d812dd2e961171e
execution_branch: master
isolation: branch
---

# aiUsageWatcher 可运行闭环 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 Plasma 6 运行链路，实现标准 KCM、供应商定义 CRUD、正确的用量语义和可重复的发布验证。

**Architecture:** `mockData.js` 维护纯函数数据边界；`main.qml` 分离 KConfig definitions、内存 snapshots 与 display model；compact 只切换自身图表，full 固定复用 ProviderGroup/PlanBar；KCM 通过 ConfigModel 和 `cfg_` 属性提交设置。

**Tech Stack:** Qt 6 QML/JavaScript、KDE Plasma 6、Kirigami、KCMUtils、QML Test、qmllint、xmllint、kpackagetool6。

## Global Constraints

- 直接在 `master` 工作，不创建或切换分支；尊重开始任务时已有未提交修改。
- 插件 ID 与安装目录唯一为 `aiUsageWatcher`。
- 唯一指标为 `usedPercent`：`<85` 绿、`85..94` 黄、`>=95` 红，无数据灰；最大值最紧张。
- Timer 只更新内存快照，不写 KConfig。
- full 始终使用 `ProviderGroup -> PlanBar`；`compactStyle` 只影响 compact。
- 仅使用 `Kirigami.Theme`、`Kirigami.Units` 和 Breeze 图标；跟随系统主题。
- 每个任务先保留 RED 输出，再实现 GREEN；`timeout` 124 不能单独作为运行成功证据。
- 每个任务完成后只提交该任务列出的文件；提交前检查 `git diff --check`。

---

### Task 1: 锁定数据契约与派生逻辑

- [x] **Task 1 checkpoint: 数据契约与派生逻辑完成**

**Files:**
- Create: `tests/tst_mockData.qml`
- Modify: `package/contents/js/mockData.js`

**Interfaces:**
- Produces: `normalizeDefinitions(raw) -> Array`、`createSeedSnapshots(definitions) -> Array`、`fluctuateSnapshots(snapshots, randomFn) -> Array`、`buildDisplayProviders(definitions, snapshots) -> Array`、`tightestUsage(displayProviders) -> { usedPercent, providerName, planName }`、`usageClass(percent, prefix) -> string`。

- [x] **Step 1: 写失败测试**

  在 `tst_mockData.qml` 使用 `QtTest.TestCase`，至少断言：84/85/94/95 分类；22/88/67 最大值为 88；MiniMax seed 为 88；null/object/坏 plans 回退；503/750 派生 67；刷新不修改输入且 `usedText`、`totalText` 与新值一致。

  ```qml
  function test_thresholds() {
      compare(MockData.usageClass(84, "bar"), "bar-green")
      compare(MockData.usageClass(85, "bar"), "bar-yellow")
      compare(MockData.usageClass(94, "bar"), "bar-yellow")
      compare(MockData.usageClass(95, "bar"), "bar-red")
  }
  function test_codex_derivation() {
      const out = MockData.buildDisplayProviders(
          MockData.SEED_PROVIDER_DEFINITIONS,
          MockData.SEED_RUNTIME_SNAPSHOTS)
      const codex = out.filter(p => p.id === "codex")[0]
      compare(codex.plans[0].usedPercent, 67)
      compare(codex.plans[0].usedText, "503")
      compare(codex.plans[0].totalText, "750")
  }
  ```

- [x] **Step 2: 运行 RED**

  Run: `QT_QPA_PLATFORM=offscreen qmltestrunner -input tests -import package/contents/ui`

  Expected: FAIL，报告 `usageClass is not a function` 或缺少新 seed 常量；保留输出到任务记录。

- [x] **Step 3: 最小实现**

  重组 seed 为 definitions 与 snapshots；`usageClass` 统一边界；`buildDisplayProviders` 从独立 used/total 派生所有展示字段；刷新只不可变更新 used，并允许测试注入 `randomFn`。provider template 由 definition 合并到每个 display plan 的 `templateText`。

- [x] **Step 4: 运行 GREEN**

  Run: `QT_QPA_PLATFORM=offscreen qmltestrunner -input tests -import package/contents/ui`

  Expected: PASS，0 failed。

- [x] **Step 5: 提交**

  ```bash
  git add tests/tst_mockData.qml package/contents/js/mockData.js
  git commit -m "test: lock runtime usage data contract"
  ```

### Task 2: 修复根状态、compact 与包元数据

- [ ] **Task 2 checkpoint: 根状态、compact 与包元数据完成**

**Files:**
- Create: `tests/tst_compactView.qml`
- Modify: `package/contents/ui/main.qml`
- Modify: `package/contents/ui/CompactView.qml`
- Modify: `package/contents/ui/PieChart.qml`
- Modify: `package/contents/config/main.xml`
- Modify: `package/metadata.json`

**Interfaces:**
- Consumes: Task 1 纯函数。
- Produces: `main.qml` 的 `providerDefinitions`、`runtimeSnapshots`、`providers`；`refresh()` 只更新内存；CompactView `tightestUsage` 属性；KConfig 四项 UI 设置和 definitions JSON。

- [ ] **Step 1: 写失败测试**

  `tst_compactView.qml` 实例化 CompactView，传入 22/88/67，断言显示 88；空数组显示 `—`；pie/bar 都可见且不抛异常。给百分比 Text 设置 `objectName: "compactPercent"` 供测试定位。

- [ ] **Step 2: 运行 RED**

  Run: `QT_QPA_PLATFORM=offscreen qmltestrunner -input tests -import package/contents/ui`

  Expected: FAIL，旧组件仍使用错误 Units/阈值或找不到 `compactPercent`。

- [ ] **Step 3: 实现根状态与 compact**

  - `main.qml` 规范化 `configuration.providers` 为 definitions，创建 snapshots 并派生 providers；`refresh()` 只替换 snapshots/providers。
  - tooltip 副标题使用 tightest provider、plan 和百分比。
  - 根 `hideOnWindowDeactivate: !keepPanelOpen`；向 FullView 传 opacity 与 keep-open 信号，不再传 compactStyle/groupBy。
  - CompactView/PieChart 全部改用 Kirigami Theme/Units，使用 Task 1 tightest 和统一颜色。
  - main.xml 只保留 `providers`、`refreshIntervalSec`、`opacityPercent`、`compactStyle`、`keepPanelOpen`。
  - metadata Name=`AI Usage Watcher`、License=`GPL-2.0-or-later`，移除外部配置模块声明。

- [ ] **Step 4: 验证 GREEN**

  Run: `QT_QPA_PLATFORM=offscreen qmltestrunner -input tests -import package/contents/ui && xmllint --noout package/contents/config/main.xml && qmllint package/contents/ui/main.qml package/contents/ui/CompactView.qml package/contents/ui/PieChart.qml`

  Expected: 全部 exit 0。

- [ ] **Step 5: 提交**

  ```bash
  git add tests/tst_compactView.qml package/contents/ui/main.qml package/contents/ui/CompactView.qml package/contents/ui/PieChart.qml package/contents/config/main.xml package/metadata.json
  git commit -m "fix: separate runtime state and repair compact view"
  ```

### Task 3: FullView 唯一组件链与响应式交互

- [ ] **Task 3 checkpoint: FullView 唯一组件链与响应式交互完成**

**Files:**
- Create: `tests/tst_fullView.qml`
- Modify: `package/contents/ui/FullView.qml`
- Modify: `package/contents/ui/ProviderGroup.qml`
- Modify: `package/contents/ui/PlanBar.qml`

**Interfaces:**
- Consumes: `DisplayProvider[]`；每个 plan 含独立 usedText/totalText/templateText。
- Produces: FullView signals `refreshRequested()`、`configureRequested()`、`keepOpenChanged(bool)`；readonly `renderedPlanCount`；ProviderGroup/PlanBar 唯一渲染树。

- [ ] **Step 1: 写失败测试**

  传入 seed display model，断言 `renderedPlanCount === 5`、`ProviderGroup` 三个、`PlanBar` 五个；分别模拟 pie/bar 配置仍得到同一树；长名称 320px 宽不覆盖操作区。为 delegate 设置 `objectName: "providerGroup"` / `"planBar"`。

- [ ] **Step 2: 运行 RED**

  Run: `QT_QPA_PLATFORM=offscreen qmltestrunner -input tests -import package/contents/ui`

  Expected: FAIL，旧 FullView 的 Loader 跨作用域引用或没有 `renderedPlanCount`。

- [ ] **Step 3: 实现唯一链路**

  删除 FullView 内联 plan Loader/Components；ListView delegate 只创建 ProviderGroup。ProviderGroup 将 provider template/plan 字段传给 PlanBar；PlanBar 用独立 `%1..%4` 渲染。使用 Layout、elide/wrap、Kirigami Theme/Units；状态栏显示刷新时间/provider/有效 plan 数。三个按钮补 ToolTip、Accessible.name，刷新 icon 通过 Rotation 动画 300ms，pin 文案为“保持面板打开”。

- [ ] **Step 4: 运行 GREEN**

  Run: `QT_QPA_PLATFORM=offscreen qmltestrunner -input tests -import package/contents/ui && qmllint package/contents/ui/FullView.qml package/contents/ui/ProviderGroup.qml package/contents/ui/PlanBar.qml`

  Expected: PASS，且 `rg -n 'planRowPie|planRowBar|planLoader' package/contents/ui/FullView.qml` 无输出。

- [ ] **Step 5: 提交**

  ```bash
  git add tests/tst_fullView.qml package/contents/ui/FullView.qml package/contents/ui/ProviderGroup.qml package/contents/ui/PlanBar.qml
  git commit -m "refactor: reuse provider and plan components in full view"
  ```

### Task 4: 标准 applet KCM 与常规设置

- [ ] **Task 4 checkpoint: 标准 applet KCM 与常规设置完成**

**Files:**
- Create: `package/contents/config/config.qml`
- Create: `package/contents/ui/config/GeneralConfig.qml`
- Create: `tests/tst_generalConfig.qml`
- Remove: `package/contents/ui/configGeneral.qml`

**Interfaces:**
- Produces: ConfigModel categories General/Providers；General properties `cfg_compactStyle`、`cfg_refreshIntervalSec`、`cfg_opacityPercent`、`cfg_keepPanelOpen`。

- [ ] **Step 1: 写失败测试**

  静态测试断言 config.qml 含两个 ConfigCategory；QML Test 实例化 GeneralConfig，改变控件后 cfg_ 属性变化，初始化 cfg_ 值时控件反映该值。

- [ ] **Step 2: 运行 RED**

  Run: `QT_QPA_PLATFORM=offscreen qmltestrunner -input tests -import package/contents/ui`

  Expected: FAIL，配置入口和 GeneralConfig 尚不存在。

- [ ] **Step 3: 实现**

  config.qml 使用 `org.kde.plasma.configuration` 的 ConfigModel/ConfigCategory，sources 为 `config/GeneralConfig.qml`、`config/ProvidersConfig.qml`。GeneralConfig 根为 KCM.SimpleKCM，使用 ComboBox/SpinBox/Slider/CheckBox 的 alias 或受控绑定暴露四个 cfg_ 属性；不直接写 Plasmoid.configuration。

- [ ] **Step 4: 运行 GREEN**

  Run: `QT_QPA_PLATFORM=offscreen qmltestrunner -input tests -import package/contents/ui && qmllint package/contents/config/config.qml package/contents/ui/config/GeneralConfig.qml`

  Expected: PASS；`rg -n 'X-KDE-ConfigModule|colorScheme|groupBy|alwaysOnTop' package` 无输出。

- [ ] **Step 5: 提交**

  ```bash
  git add package/contents/config/config.qml package/contents/ui/config/GeneralConfig.qml tests/tst_generalConfig.qml package/contents/ui/configGeneral.qml
  git commit -m "feat: add standard Plasma general configuration"
  ```

### Task 5: Providers KCM CRUD、校验与模板预览

- [ ] **Task 5 checkpoint: Providers KCM CRUD、校验与模板预览完成**

**Files:**
- Create: `package/contents/ui/config/ProvidersConfig.qml`
- Create: `package/contents/ui/config/ProviderEditor.qml`
- Create: `package/contents/js/providerConfig.js`
- Create: `tests/tst_providerConfig.qml`

**Interfaces:**
- Produces: `validateProvider(candidate, siblings) -> { valid, message }`、`parseWorkingDefinitions(json) -> Array`、`serializeDefinitions(items) -> string`；ProvidersConfig `property string cfg_providers`。

- [ ] **Step 1: 写失败测试**

  覆盖空名称、重复 provider ID、零 plan、空/重复 plan、缺模板占位符为 invalid；合法 add/edit/delete 更新工作 JSON；Cancel 场景不触碰模拟 persisted 值；模板示例输出 `5小时 限额  65/100  重置于 今天 18:00`。

- [ ] **Step 2: 运行 RED**

  Run: `QT_QPA_PLATFORM=offscreen qmltestrunner -input tests -import package/contents/ui`

  Expected: FAIL，providerConfig.js/ProvidersConfig 尚不存在。

- [ ] **Step 3: 实现**

  ProvidersConfig 从 cfg_providers 建 ListModel 工作副本；单个 Controls.Dialog 承载普通 ProviderEditor Item。所有查找按稳定 ID；保存启用状态来自 validateProvider；删除使用二次确认。编辑后只序列化到 cfg_providers。按钮使用 list-add/document-edit/edit-delete Breeze icon；脚本区只读提示后续版本；模板预览实时更新。

- [ ] **Step 4: 运行 GREEN**

  Run: `QT_QPA_PLATFORM=offscreen qmltestrunner -input tests -import package/contents/ui && qmllint package/contents/ui/config/ProvidersConfig.qml package/contents/ui/config/ProviderEditor.qml`

  Expected: PASS；`rg -n 'plasmoid\.configuration|Plasmoid\.configuration' package/contents/ui/config` 无输出。

- [ ] **Step 5: 提交**

  ```bash
  git add package/contents/ui/config/ProvidersConfig.qml package/contents/ui/config/ProviderEditor.qml package/contents/js/providerConfig.js tests/tst_providerConfig.qml
  git commit -m "feat: add validated provider configuration workflow"
  ```

### Task 6: 自动验证与安装级冒烟

- [ ] **Task 6 checkpoint: 自动验证与安装级冒烟完成**

**Files:**
- Create: `tests/run-static-checks.sh`
- Create: `tests/run-plasma-smoke.sh`
- Create: `tests/README.md`

**Interfaces:**
- Produces: 一个静态入口和一个安装/运行入口；脚本返回 0 仅当对应全部断言通过。

- [ ] **Step 1: 写失败脚本**

  `run-static-checks.sh` 执行所有 qmltestrunner、qmllint、xmllint、metadata 非空字段和禁用模式扫描。`run-plasma-smoke.sh` 使用 `kpackagetool6 --type Plasma/Applet --upgrade package`，`diff -qr` 检查副本，启动自身记录的 plasmawindowed PID，等待固定窗口后用该 PID 正常结束并审查日志；不得用全局进程匹配命令。

- [ ] **Step 2: 运行 RED**

  Run: `bash tests/run-static-checks.sh && bash tests/run-plasma-smoke.sh`

  Expected: 首次至少一项失败，记录精确断言；若全绿，向脚本注入一份临时 fixture 验证错误扫描确实能失败，不改项目文件。

- [ ] **Step 3: 完成验证实现**

  runtime 脚本必须拒绝包含 `ReferenceError`、`TypeError`、`PlasmaCore.Units`、`Error loading QML file` 的日志；结合 `tst_fullView` 的 3 provider/5 PlanBar 断言，并在 `tests/README.md` 规定人工打开 popup 截图核对五条套餐。保存进程 exit 状态与日志结果；124 只在日志和可见验证同时通过时记录为“持续运行”，脚本本身返回 0。

- [ ] **Step 4: 运行 GREEN**

  Run: `bash tests/run-static-checks.sh && bash tests/run-plasma-smoke.sh`

  Expected: exit 0；安装 diff 无输出；runtime forbidden patterns 为 0；人工证据看到三 provider/五 PlanBar。

- [ ] **Step 5: 提交**

  ```bash
  git add tests/run-static-checks.sh tests/run-plasma-smoke.sh tests/README.md
  git commit -m "test: add Plasma package release gates"
  ```

### Task 7: 文档同步与最终验收

- [ ] **Task 7 checkpoint: 文档同步与最终验收完成**

**Files:**
- Modify: `README.md`
- Modify: `docs/requirements.md`
- Modify: `docs/usage-script-spec.md`
- Modify: `docs/superpowers/specs/2026-07-21-kcm-and-refresh-button-design.md`
- Modify: `openspec/changes/minimal-viable-plasmoid/tasks.md`
- Modify: `openspec/changes/minimal-viable-plasmoid/.comet/subagent-progress.md`

**Interfaces:**
- Consumes: Tasks 1-6 的真实命令结果。
- Produces: 与实现一致的安装、架构、测试说明和完成状态。

- [ ] **Step 1: 建立文档一致性失败检查**

  Run: `rg -n 'org\.kde\.plasma\.aiUsageWatcher|kpackagetool6 --install aiUsageWatcher|PlasmaCore\.(Units|Theme)|≤5%|≤15%|X-KDE-ConfigModule' README.md docs openspec`

  Expected: 在旧 README/AGENTS 或历史文档发现过期描述并记录。

- [ ] **Step 2: 同步文档与完成状态**

  README 使用真实文件树、`aiUsageWatcher` 安装命令、definitions/snapshots/display 数据流和测试入口。只在对应 GREEN 证据存在时勾选 requirements/OpenSpec/subagent ledger；保留未完成项为未勾选并写明风险。

- [ ] **Step 3: 最终验证**

  Run: `bash tests/run-static-checks.sh && bash tests/run-plasma-smoke.sh && git diff --check && git status --short`

  Expected: 前三项 exit 0；status 只显示本计划范围内预期改动；人工 popup/KCM 验证完成。

- [ ] **Step 4: 提交**

  ```bash
  git add README.md docs/requirements.md docs/usage-script-spec.md docs/superpowers/specs/2026-07-21-kcm-and-refresh-button-design.md openspec/changes/minimal-viable-plasmoid/tasks.md openspec/changes/minimal-viable-plasmoid/.comet/subagent-progress.md
  git commit -m "docs: align plasmoid architecture and verification"
  ```

## Final Review Gate

- [ ] `usedPercent` 语义、模板字段和配置名称在代码、测试和文档中一致。
- [ ] FullView 没有第二套 plan UI；KCM 没有直接配置写入或外部模块声明。
- [ ] Timer 不写 KConfig；安装副本与 package 一致。
- [ ] 自动验证全部 GREEN，真实运行日志无运行时错误，三 provider/五 PlanBar 可见。
- [ ] 审查 `git diff ebbf3df2892413a10c64a2637d812dd2e961171e..HEAD` 和任务开始前未提交改动，未覆盖用户内容。
