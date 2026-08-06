# Plasma 原生图表改造 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让项目内饼图和水平用量条分别使用与 Plasma CPU/内存、硬盘监控相同的原生绘制组件。

**Architecture:** 配额数据流和颜色语义保持不变；`CompactView` 与 `PanelPieView` 使用 `Charts.PieChart`，`CompactView` 与 `PlanBar` 使用完全覆写内容和背景的 `QQC2.ProgressBar`。测试只观察稳定的组件值、范围、语义色、底轨与无数据行为，不保留旧 Shape 内部对象。

**Tech Stack:** KDE Plasma 6、Qt Quick 6、KQuickCharts、Qt Quick Controls 2、QML Test

## Global Constraints

- 饼图必须使用 `org.kde.quickcharts` 的 `Charts.PieChart`，与 Plasma CPU/内存监控相同。
- 水平用量条必须使用 `QQC2.ProgressBar` 自定义 `contentItem/background`，与 Plasma 硬盘监控相同。
- 禁止 Canvas、`QtQuick.Shapes`、SVG 图表和自定义 Shader。
- 静态门必须覆盖现有全部图表文件：`CompactView.qml`、`PanelPieView.qml`、
  `PlanBar.qml`，并验证它们只使用上述原生组件。
- 不改变数据模型、配置格式、百分比/颜色语义和布局；水平用量条保留 300ms
  `Easing.OutCubic` 动画，饼图直接跟随 Plasma 原生 `Charts.PieChart` 更新方式。
- 直接在当前 checkout 实施，不创建 worktree，不覆盖工作区既有改动。

---

### Task 1: CompactView 原生图表契约

**Files:**
- Modify: `tests/tst_compactView.qml`
- Modify: `package/contents/ui/CompactView.qml`

**Interfaces:**
- Consumes: `root.boundedPercent`、`root.currentUsage.usedPercent`、`root.usageColor(percent)`
- Produces: `compactPieChart`、`compactPieValueSource`、`compactPieColorSource`、
  `compactProgressBar`、`compactProgressFill`、`compactBarTrack` 稳定测试对象

- [x] **Step 1: 写入失败测试**

在 `tst_compactView.qml` 用 `findChild` 验证：

```qml
const chart = findChild(compact, "compactPieChart")
const source = findChild(compact, "compactPieValueSource")
verify(chart !== null)
compare(source.value, 50)
compare(chart.range.from, 0)
compare(chart.range.to, 100)
compare(findChild(compact, "compactPieColorSource").value,
        compact.usageColor(50))

const progress = findChild(compact, "compactProgressBar")
compare(progress.from, 0)
compare(progress.to, 100)
compare(progress.value, 50)
compare(findChild(progress, "compactProgressFill").color,
        compact.usageColor(50))
verify(findChild(progress, "compactBarTrack").width > 0)
```

分别设置 `usedPercent` 为 `0`、`50`、`100` 和空数据，验证值钳制、`—` 文本，
以及正常值和无数据分别使用现有语义色与禁用色。

- [x] **Step 2: 验证测试先失败**

Run:

```bash
QT_QPA_PLATFORM=offscreen /usr/lib/qt6/bin/qmltestrunner \
  -input tests/tst_compactView.qml -import package/contents/ui
```

Expected: FAIL，旧实现不存在 `compactPieChart` 或 `compactProgressBar`。

- [x] **Step 3: 替换 compact 饼图**

删除 `import QtQuick.Shapes`，添加：

```qml
import org.kde.quickcharts as Charts
```

以以下结构替换 `Shape/PathAngleArc`：

```qml
Charts.PieChart {
    objectName: "compactPieChart"
    anchors.fill: parent
    valueSources: Charts.SingleValueSource {
        objectName: "compactPieValueSource"
        value: root.currentUsage.usedPercent >= 0 ? root.boundedPercent : 0
    }
    colorSource: Charts.SingleValueSource {
        objectName: "compactPieColorSource"
        value: root.usageColor(root.currentUsage.usedPercent)
    }
    range { from: 0; to: 100; automatic: false }
    thickness: pieFace.thickness
    backgroundColor: Kirigami.ColorUtils.linearInterpolation(
                         Kirigami.Theme.backgroundColor,
                         Kirigami.Theme.textColor, 0.15)
    smoothEnds: true
}
```

- [x] **Step 4: 替换 compact 水平条**

以以下层级替换自定义条：

```qml
QQC2.ProgressBar {
    id: compactProgress
    objectName: "compactProgressBar"
    from: 0
    to: 100
    value: root.currentUsage.usedPercent >= 0 ? root.boundedPercent : 0
    contentItem: Item {
        Rectangle {
            objectName: "compactProgressFill"
            anchors.left: parent.left
            height: parent.height
            width: parent.width * compactProgress.visualPosition
            radius: height / 2
            color: root.usageColor(root.currentUsage.usedPercent)
            Behavior on width {
                NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
            }
        }
    }
    background: Rectangle {
        objectName: "compactBarTrack"
        radius: height / 2
        color: Kirigami.Theme.disabledTextColor
        opacity: 0.28
    }
}
```

保留原布局尺寸、可访问名称和描述。

- [x] **Step 5: 运行 compact 测试**

Run: 与 Step 2 相同。

Expected: PASS。

### Task 2: Popup 水平条原生契约

**Files:**
- Modify: `tests/tst_providerGroup.qml`
- Modify: `package/contents/ui/PlanBar.qml`
- Audit only: `package/contents/ui/PanelPieView.qml`

**Interfaces:**
- Consumes: `usedPercent`、`barClass`、`usageColor(usageClass)`
- Produces: `planProgressBar`、`planProgressFill`、`unusedTrack`

- [x] **Step 1: 写入失败测试**

在 `tst_providerGroup.qml` 找到 `planBar` 后验证：

```qml
const progress = findChild(plan, "planProgressBar")
verify(progress !== null)
compare(progress.from, 0)
compare(progress.to, 100)
compare(progress.value, 50)
compare(findChild(progress, "planProgressFill").color,
        plan.usageColor(plan.barClass))
verify(findChild(progress, "unusedTrack").width > 0)
```

将套餐输入切换为 `0`、`100`、`-1`，分别验证 `value` 为 `0`、`100`、`0`；
无数据用例同时提供 `usedPercentLabel: "—"`，验证组件原样展示，不假定
`PlanBar` 负责从数值派生标签。

- [x] **Step 2: 验证测试先失败**

Run:

```bash
QT_QPA_PLATFORM=offscreen /usr/lib/qt6/bin/qmltestrunner \
  -input tests/tst_providerGroup.qml -import package/contents/ui
```

Expected: FAIL，旧实现不存在 `planProgressBar`。

- [x] **Step 3: 替换 PlanBar 水平条**

用与 Task 1 相同的 `QQC2.ProgressBar` 层级替换现有 `Item`，其中：

```qml
objectName: "planProgressBar"
from: 0
to: 100
value: root.usedPercent >= 0 ? Math.max(0, Math.min(100, root.usedPercent)) : 0
```

背景保留 `objectName: "unusedTrack"`；填充色使用
`root.usageColor(root.barClass)`，填充项保留 `objectName: "planProgressFill"`，
宽度动画保持 300ms `Easing.OutCubic`。

- [x] **Step 4: 审计 popup 饼图**

确认 `PanelPieView.qml` 仍包含：

```qml
Charts.PieChart
Charts.SingleValueSource
range { from: 0; to: 100; automatic: false }
smoothEnds: true
```

不得修改已符合要求的实现。

- [x] **Step 5: 运行 popup 测试**

Run: 与 Step 2 相同。

Expected: PASS。

### Task 3: 全量验证与视觉验收

**Files:**
- Modify: `tests/README.md`
- Modify: `tests/run-static-checks.sh`

**Interfaces:**
- Consumes: Task 1、Task 2 的稳定对象契约
- Produces: 可重复的自动测试和 Plasma 缩放验收说明

- [x] **Step 1: 增加静态禁用模式**

在 `tests/run-static-checks.sh` 增加现有全部图表文件的约束：

```text
CompactView.qml   -> 必须包含 Charts.PieChart 和 QQC2.ProgressBar
PanelPieView.qml  -> 必须包含 Charts.PieChart
PlanBar.qml       -> 必须包含 QQC2.ProgressBar
上述三个文件      -> 禁止 QtQuick.Shapes|PathAngleArc|Canvas|ShaderEffect
```

- [x] **Step 2: 运行自动验证**

Run:

```bash
bash tests/run-static-checks.sh
```

Expected: 相关 QML 测试与全部生产 QML 的 `qmllint` 必须 PASS。若全量脚本被既有
`mockData.js` 引用阻断，记录为“范围内验证通过、仓库基线未通过”，不得写成完整 PASS。

- [x] **Step 3: 运行安装级冒烟**

Run:

```bash
bash tests/run-plasma-smoke.sh
```

Expected: PASS。若缺少可见 Plasma 会话则明确返回 `BLOCKED`，此时只能标记
“代码完成、视觉待验”，不得完成本计划。

- [x] **Step 4: 更新人工验收说明**

在 `tests/README.md` 增加：compact/popup × pie/bar × 亮/暗主题 ×
100%/125%/150% 缩放，核对无明显锯齿、裁切、空白图表、100% 端点缝隙，
且 0%/无数据底轨可见。保存截图并通过该检查后，方可宣称视觉改造完成。

- [x] **Step 5: 检查范围**

Run:

```bash
git diff --check
git diff -- package/contents/ui/CompactView.qml package/contents/ui/PlanBar.qml \
  tests/tst_compactView.qml tests/tst_providerGroup.qml tests/run-static-checks.sh \
  tests/README.md
```

Expected: 仅包含本计划图表、测试和验收说明的外科手术式改动。

## GSTACK REVIEW REPORT

| Review | Trigger | Why | Runs | Status | Findings |
|--------|---------|-----|------|--------|----------|
| CEO Review | `/plan-ceo-review` | Scope & strategy | 0 | — | 本次不改变产品范围 |
| Codex Review | `/codex review` | Independent 2nd opinion | 2 | CLEAR (fallback) | CLI 受权限策略阻断；独立只读审查的 3 项验收缺口已折叠 |
| Eng Review | `/plan-eng-review` | Architecture & tests (required) | 2 | CLEAR | 6 项设计缺口已修正，0 个关键缺口 |
| Design Review | `/plan-design-review` | UI/UX gaps | 0 | — | 真实 Plasma 截图门负责最终视觉验收 |
| DX Review | `/plan-devex-review` | Developer experience gaps | 0 | — | 不适用 |

**CODEX:** CLI 复核受权限策略阻断；独立回退审查补齐了全局图表约束、颜色测试契约和测试基线表述。

**VERDICT:** ENG CLEARED — 可实施；真实 Plasma 视觉门通过后才能宣称改造完成。

NO UNRESOLVED DECISIONS
