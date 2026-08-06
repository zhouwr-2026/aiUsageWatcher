# CodexZH 悬浮面板两段用量 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 CodexZH 展开面板把本周已用额度显示为“此前使用”和“今日使用”两段，并在水平条与圆环上提供正确提示。

**Architecture:** C++ 以强类型可选数值区分真实零值和缺失数据，并生成无展示语义的 `usageSegments`。展示模型只为 `codexzh/weekly` 透传有效段；QML 负责标签、主题色和固定角度命中。CompactView 不接收分段逻辑。

**Tech Stack:** C++17、Qt 6、QML、Qt Quick Controls、Kirigami、KQuickCharts、Qt Test、qmltestrunner。

## Global Constraints

- 仅 `providerId === "codexzh"` 且 `planId === "weekly"` 可显示分段。
- 计算只用 `weekUsed` 和原始 `todayUsed`；`todayUsedFormatted` 只用于今日金额文本。
- 不新增历史、KConfig、后台任务、第三方依赖、Canvas、Shapes 或 Shader。
- 条形图继续使用 `QQC2.ProgressBar`，圆环继续使用 `Charts.PieChart`；不改 `CompactView.qml`。

---

### Task 1: 强类型解析和两段领域数据

**Files:**
- Modify: `src/codexzhresponseparser.h:10-18`
- Modify: `src/codexzhresponseparser.cpp:17-223,280-319`
- Test: `tests/cpp/tst_codexzhresponseparser.cpp:14-176`

**Interfaces:**
- Produces: `CodexZhUsageSegment { QString kind; double used; double usedPercent; QString formattedUsed; }`。
- Produces: `QList<CodexZhUsageSegment> CodexZhPlan::usageSegments`。

- [ ] **Step 1: 写失败的 C++ 测试**

新增 `parsesWeeklyUsageSegments()`、`omitsSegmentsWhenTodayUsedIsMissingOrInvalid()`、`clampsTodayUsageToWeeklyUsage()`。对 `weekUsed:50,todayUsed:30` 断言 `previous:20`、`today:30`；分别覆盖数字字符串、`todayUsed:0`、字段缺失、`"not-a-number"`、负数与 `todayUsed:80`。后四种无效输入断言空段，80 断言仅 today 50。

- [ ] **Step 2: 运行失败测试**

Run: `cmake --build build --target tst_codexzhresponseparser && ctest --test-dir build -R codexzh-response-parser --output-on-failure`

Expected: 编译失败，提示分段接口尚不存在。

- [ ] **Step 3: 实现最小强类型解析**

定义：

```cpp
struct CodexZhUsageSegment {
    QString kind;
    double used = 0;
    double usedPercent = 0;
    QString formattedUsed;
};
```

内部数值函数返回 `std::optional<double>`：接受 JSON 数字，或去掉 `$`、`,`、空白后可解析的字符串；空值、非有限值和其他类型返回 `std::nullopt`。详情文本在调用点显式使用 `value_or(0.0)`，周额度与 `weekUsed` 必须有效。仅当 today 为非负有效值时计算：

```cpp
const double clampedToday = qBound(0.0, *todayUsed, weekUsed);
const double previous = weekUsed - clampedToday;
```

为正数的 `previous` 和 `clampedToday` 依次写入段；today 的金额文本复用 `formattedOrUsd()`。不传中文标签或颜色。

- [ ] **Step 4: 运行通过测试并提交**

Run: `cmake --build build --target tst_codexzhresponseparser && ctest --test-dir build -R codexzh-response-parser --output-on-failure`

Expected: `codexzh-response-parser` 通过，既有 16 项详情文本断言不变。

```bash
git add src/codexzhresponseparser.h src/codexzhresponseparser.cpp tests/cpp/tst_codexzhresponseparser.cpp
git commit -m "feat: derive CodexZH usage segments"
```

### Task 2: 快照映射和展示层白名单

**Files:**
- Modify: `src/codexzhclient.cpp:30-50`
- Modify: `package/contents/js/displayProvider.js:156-183`
- Test: `tests/tst_mockData.qml`

**Interfaces:**
- Consumes: `CodexZhPlan::usageSegments`。
- Produces: QML 计划字段 `usageSegments`，仅限有效 `codexzh/weekly`。

- [ ] **Step 1: 写失败的展示模型测试**

在 `tests/tst_mockData.qml` 添加：有效 CodexZH weekly 透传两段；未知 `kind`、负 `usedPercent`、非数组不透传；其他供应商即使有同名字段也不透传。断言：

```qml
compare(codexZh.plans[0].usageSegments.length, 2)
verify(!otherProvider.plans[0].hasOwnProperty("usageSegments"))
verify(!invalidSegmentsProvider.plans[0].hasOwnProperty("usageSegments"))
```

- [ ] **Step 2: 运行失败测试**

Run: `QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=org.kde.desktop /usr/lib/qt6/bin/qmltestrunner -input tests/tst_mockData.qml -import package/contents/ui`

Expected: 有效段未透传，或越界供应商未被拒绝。

- [ ] **Step 3: 映射并校验数据**

`toVariantMap()` 将每段映射为 `kind`、`used`、`usedPercent`、`formattedUsed`。`displayProvider.js` 增加私有校验：`definition.id === "codexzh"`、`snapshotPlan.planId === "weekly"`、数组存在、kind 仅为 previous/today、金额和百分比有限且大于 0。校验通过后复制数组；失败时不返回该属性。不得按名称判断。

- [ ] **Step 4: 运行通过测试并提交**

Run: `QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=org.kde.desktop /usr/lib/qt6/bin/qmltestrunner -input tests/tst_mockData.qml -import package/contents/ui`

Expected: 只有 CodexZH weekly 保留分段。

```bash
git add src/codexzhclient.cpp package/contents/js/displayProvider.js tests/tst_mockData.qml
git commit -m "feat: expose validated CodexZH segments"
```

### Task 3: 原生水平条分段与提示

**Files:**
- Modify: `package/contents/ui/ProviderGroup.qml:162-181`
- Modify: `package/contents/ui/PlanBar.qml:10-120`
- Test: `tests/tst_fullView.qml`

**Interfaces:**
- Consumes: 展示计划可选 `usageSegments`。
- Produces: `PlanBar` 内的 `previousUsageSegment`、`todayUsageSegment` 与提示。

- [ ] **Step 1: 写失败的 QML 测试**

向 CodexZH 测试快照写入 20%/30% 分段。断言两个 `usageSegment`、顺序、宽度合计等于已使用宽度、`todayUsageSegment.Accessible.name === "今日使用 · 30% · $30.00"`。再断言无 `usageSegments` 时只有原单色填充，零段不创建对象。

- [ ] **Step 2: 运行失败测试**

Run: `QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=org.kde.desktop /usr/lib/qt6/bin/qmltestrunner -input tests/tst_fullView.qml -import package/contents/ui`

Expected: 找不到分段对象。

- [ ] **Step 3: 使用现有 ProgressBar 绘制**

`ProviderGroup.qml` 传 `usageSegments: modelData.usageSegments || []`。`PlanBar.qml` 新增 `property var usageSegments: []`；无段时保留现有单色矩形，有段时在同一 `contentItem` 用 `Repeater` 连续绘制。previous 用 `Kirigami.Theme.highlightColor`，today 用 `Kirigami.Theme.positiveTextColor`。每段宽度为 `parent.width * usedPercent / 100`，左边界为前段宽度累计。用 `HoverHandler` + `PlasmaComponents.ToolTip`，标签按 kind 在 QML 中生成，百分比最多两位小数。

- [ ] **Step 4: 运行通过测试并提交**

Run: `QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=org.kde.desktop /usr/lib/qt6/bin/qmltestrunner -input tests/tst_fullView.qml -import package/contents/ui`

Expected: 宽度、顺序、提示、无障碍名称和单色回退通过。

```bash
git add package/contents/ui/ProviderGroup.qml package/contents/ui/PlanBar.qml tests/tst_fullView.qml
git commit -m "feat: render segmented CodexZH progress bars"
```

### Task 4: 原生圆环分段和固定命中

**Files:**
- Modify: `package/contents/ui/PanelPieView.qml:1-107`
- Test: `tests/tst_fullView.qml`

**Interfaces:**
- Produces: `usageSegmentAt(x, y)`，返回分段索引或 `-1`。

- [ ] **Step 1: 写失败的圆环测试**

在 pie 模式找到 CodexZH weekly 圆环包装项，断言 `usageSegmentAt()` 存在。对 20%/30% 固定坐标分别断言此前、今日命中为 0、1；中心、图表外部和未使用角度均为 -1。

- [ ] **Step 2: 运行失败测试**

Run: `QT_QPA_PLATFORM=offscreen QT_QUICK_CONTROLS_STYLE=org.kde.desktop /usr/lib/qt6/bin/qmltestrunner -input tests/tst_fullView.qml -import package/contents/ui`

Expected: `usageSegmentAt` 不存在。

- [ ] **Step 3: 用 ArraySource 和显式角度实现**

圆环包装 Item 定义 `usageSegmentAt(x, y)`：先检查内外半径，再换算成从 12 点钟方向起、顺时针增加的 0..360 度，按累计 `usedPercent * 3.6` 定位，未使用区返回 -1。`PieChart` 显式使用：

```qml
fromAngle: -90
toAngle: 270
valueSources: hasSegments ? Charts.ArraySource { array: segmentPercents }
                          : Charts.SingleValueSource { value: usedPercent }
colorSource: hasSegments ? Charts.ArraySource { array: segmentColors }
                         : Charts.SingleValueSource { value: root.usageColor(usedPercent) }
```

包装项的 `HoverHandler` 将有效索引映射到与列表图相同的 ToolTip 与无障碍文本；无段时保留单值圆环。

- [ ] **Step 4: 运行静态门并提交**

Run: `bash tests/run-static-checks.sh`

Expected: QML 测试、qmllint 与原生图表门通过。

```bash
git add package/contents/ui/PanelPieView.qml tests/tst_fullView.qml
git commit -m "feat: render segmented CodexZH pie charts"
```

### Task 5: 完整验证和范围检查

**Files:**
- Verify only: `package/contents/ui/CompactView.qml`
- Verify only: `tests/run-static-checks.sh`
- Verify only: `tests/run-plasma-smoke.sh`

- [ ] **Step 1: 构建并运行 C++ 测试**

Run: `cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local" && cmake --build build && ctest --test-dir build --output-on-failure`

Expected: 构建成功，所有 C++ 测试通过。

- [ ] **Step 2: 运行静态和 QML 测试门**

Run: `bash tests/run-static-checks.sh`

Expected: qmltestrunner、qmllint、KConfig 和原生 Charts 门通过。

- [ ] **Step 3: 检查范围和顶部面板回归**

Run: `git diff --check && git diff --name-only`

Expected: 不包含 `package/contents/ui/CompactView.qml`，也不包含 KConfig、历史记录或依赖改动。

- [ ] **Step 4: 在 Plasma 会话运行安装级冒烟**

Run: `bash tests/run-plasma-smoke.sh`

Expected: 展开列表和饼图均可加载；没有可见 Plasma 会话时记录 `BLOCKED`，不能当作通过。

## Plan Self-Review

- 每项设计约束均映射到一个实施任务和明确测试。
- 分段名称、颜色、提示留在 QML；解析层不含展示语义。
- Task 1 → 2 → 3/4 → 5；Task 3 和 4 可在 Task 2 后并行，但共享 `tst_fullView.qml`，实际修改应顺序合并。
