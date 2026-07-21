---
change: minimal-viable-plasmoid
design-doc: docs/superpowers/specs/2026-07-20-minimal-viable-plasmoid-design.md
base-ref: f39ad13858e21a0e8050ab0dfd7f9de71430941d
---

# 最小可行小部件 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 aiUsageWatcher 小部件可实际运行——mock 数据驱动、Timer 定时刷新、修复全部已知 bug，通过 `plasmawindowed` 验证。

**Architecture:** 新建 `package/contents/js/mockData.js` 提供种子数据和波动函数；main.qml 引入 mockData 并添加 Timer 每 60 秒刷新；同时修复 ProviderGroup 重复 border.color、错误标签可见性、供应商名后缀剥离、Orb 灰色态 4 个 bug。

**Tech Stack:** QML (Qt 6 / Plasma 6)、JavaScript (QML 内嵌引擎)、plasmawindowed 验证

## Global Constraints

- Plasma 6 / Qt 6 QML 语法（无 Qt 5 兼容）
- 小部件 ID: `aiUsageWatcher`，包结构遵循 `Plasma/Applet` KPackageStructure
- mock 数据结构必须与未来 DisplayQuota 对齐：`{providerName, ledClass, sourceLabel, statusLabel, errorText, plans: [{planName, usedPercent, usedPercentLabel, barClass, resetText, usedText, unitText, extraText}]}`
- 颜色语义：usedPercent ≤ 5% → 红，≤ 15% → 黄，> 15% → 绿，无 plan → 灰
- 不可变更新 providers 数组以触发 QML 绑定刷新
- 本次不实现：KConfig 读写、KWallet、HTTP 请求、KCM 配置页、真实供应商查询

---

## File Structure

| 文件 | 操作 | 职责 |
|------|------|------|
| `package/contents/js/mockData.js` | **创建** | 种子供应商数据、stripProviderSuffix()、fluctuateProviders() |
| `package/contents/ui/main.qml` | 修改 | 引入 mockData、添加 Timer、替换内联数据、修复 tightestProviderName() |
| `package/contents/ui/ProviderGroup.qml` | 修改 | 删除重复 border.color、修复错误标签可见性 |
| `package/contents/ui/Orb.qml` | 修改 | 无需改动（main.qml 已处理灰色态映射） |
| `package/contents/ui/PlanBar.qml` | 不变 | — |
| `package/contents/ui/configGeneral.qml` | 修改 | 移除无效 import（当前为空壳，无 import 可移除，确认后跳过） |
| `README.md` | 修改 | 添加 plasmawindowed 开发指令 |

---

### Task 1: 创建 mockData.js — 种子数据与工具函数

**Files:**
- Create: `package/contents/js/mockData.js`

**Interfaces:**
- Consumes: 无（独立模块）
- Produces:
  - `MockData.SEED_PROVIDERS` — `var` 数组，3 个种子供应商对象
  - `MockData.stripProviderSuffix(name)` — `(string) → string`，去除 " · xxx" 后缀
  - `MockData.fluctuateProviders(providers)` — `(Provider[]) → Provider[]`，返回新数组，每个 plan 的 usedPercent ± random(0-5)

- [ ] **Step 1: 创建 js 目录**

```bash
mkdir -p package/contents/js
```

- [ ] **Step 2: 创建 mockData.js 完整内容**

```javascript
// package/contents/js/mockData.js
// 种子供应商数据 + 波动函数，供 main.qml Timer 驱动刷新

.pragma library

var SEED_PROVIDERS = [{
    "providerName": "云之声Token Hub",
    "ledClass": "led-green",
    "sourceLabel": "自定义",
    "statusLabel": "可用",
    "errorText": "",
    "plans": [{
        "planName": "5小时",
        "usedPercent": 65,
        "usedPercentLabel": "65%",
        "barClass": "bar-green",
        "resetText": "今天 18:00",
        "usedText": "141775516 / 180000000",
        "unitText": "",
        "extraText": ""
    }, {
        "planName": "7天",
        "usedPercent": 22,
        "usedPercentLabel": "22%",
        "barClass": "bar-green",
        "resetText": "周日 00:00",
        "usedText": "",
        "unitText": "",
        "extraText": ""
    }, {
        "planName": "30天",
        "usedPercent": 8,
        "usedPercentLabel": "8%",
        "barClass": "bar-green",
        "resetText": "",
        "usedText": "",
        "unitText": "",
        "extraText": ""
    }]
}, {
    "providerName": "MiniMax · Claude",
    "ledClass": "led-yellow",
    "sourceLabel": "套餐",
    "statusLabel": "降级",
    "errorText": "",
    "plans": [{
        "planName": "余额",
        "usedPercent": 88,
        "usedPercentLabel": "88%",
        "barClass": "bar-yellow",
        "resetText": "",
        "usedText": "12.5 / 100",
        "unitText": "$",
        "extraText": "活动期 8 月底结束"
    }]
}, {
    "providerName": "Codex",
    "ledClass": "led-green",
    "sourceLabel": "订阅",
    "statusLabel": "可用",
    "errorText": "",
    "plans": [{
        "planName": "周限额",
        "usedPercent": 67,
        "usedPercentLabel": "67%",
        "barClass": "bar-green",
        "resetText": "周日 00:00",
        "usedText": "503/750 次",
        "unitText": "",
        "extraText": ""
    }]
}];

// 去除供应商名中的 " · xxx" 后缀（如 "MiniMax · Claude" → "MiniMax"）
function stripProviderSuffix(name) {
    if (typeof name !== "string")
        return "";

    const idx = name.indexOf(" · ");
    return idx >= 0 ? name.substring(0, idx) : name;
}

// 根据 usedPercent 计算 barClass
function _barClass(pct) {
    if (pct <= 5)
        return "bar-red";

    if (pct <= 15)
        return "bar-yellow";

    return "bar-green";
}

// 根据 plans 中最紧张值计算 ledClass
function _ledClass(plans) {
    let worst = -1;
    for (const plan of plans) {
        if (typeof plan.usedPercent === "number" && plan.usedPercent > worst)
            worst = plan.usedPercent;

    }
    if (worst < 0)
        return "led-gray";

    if (worst <= 5)
        return "led-red";

    if (worst <= 15)
        return "led-yellow";

    return "led-green";
}

// 对 providers 数组做不可变波动：每个 plan 的 usedPercent ± random(0-5)
// 返回新数组以触发 QML 绑定
function fluctuateProviders(providers) {
    return providers.map(function (p) {
        const newPlans = p.plans.map(function (plan) {
            const delta = Math.random() * 10 - 5; // -5 ~ +5
            const newPct = Math.max(0, Math.min(100, Math.round(plan.usedPercent + delta)));
            return Object.assign({}, plan, {
                "usedPercent": newPct,
                "usedPercentLabel": newPct + "%",
                "barClass": _barClass(newPct)
            });
        });
        return Object.assign({}, p, {
            "plans": newPlans,
            "ledClass": _ledClass(newPlans)
        });
    });
}
```

- [ ] **Step 3: 验证文件创建**

```bash
ls -la package/contents/js/mockData.js
```

Expected: 文件存在，大小约 2.5KB

- [ ] **Step 4: 提交**

```bash
git add package/contents/js/mockData.js
git commit -m "feat: add mockData.js with seed providers and fluctuate function"
```

---

### Task 2: 修复 ProviderGroup.qml — 重复 border.color 和错误标签可见性

**Files:**
- Modify: `package/contents/ui/ProviderGroup.qml:22-36` (重复 border.color)
- Modify: `package/contents/ui/ProviderGroup.qml:106` (错误标签可见性)

**Interfaces:**
- Consumes: 无外部依赖
- Produces: ProviderGroup 组件行为修正（供 main.qml Repeater 使用）

- [ ] **Step 1: 删除重复的 border.color 赋值**

ProviderGroup.qml 第 22 行有一个静态 `border.color: Qt.rgba(1, 1, 1, 0.08)`，第 25 行又有一个 switch 表达式的 `border.color`。后者覆盖前者，前者无效。删除前者。

将第 22 行：
```qml
    border.color: Qt.rgba(1, 1, 1, 0.08)
```
删除（整行移除），保留第 25 行的 switch 表达式。

修改后第 22-36 行应为：
```qml
    border.width: 1
    // 整体边框颜色随状态切换
    border.color: {
        switch (ledClass) {
        case "led-green":
            return Qt.rgba(0.2, 0.82, 0.6, 0.2);
        case "led-yellow":
            return Qt.rgba(0.98, 0.75, 0.14, 0.2);
        case "led-red":
            return Qt.rgba(0.97, 0.44, 0.44, 0.3);
        default:
            return Qt.rgba(1, 1, 1, 0.08);
        }
    }
```

- [ ] **Step 2: 修复错误标签可见性条件**

将第 106 行：
```qml
            visible: groupRoot.errorText.length > 0 && groupRoot.plans.length === 0
```
改为：
```qml
            visible: groupRoot.errorText.length > 0
```

设计文档明确：只要有 errorText 就应显示错误信息，不要求 plans 为空。这样即使供应商有部分 plan 返回数据但仍有错误，错误信息也能展示。

- [ ] **Step 3: 验证修改**

```bash
grep -n "border.color" package/contents/ui/ProviderGroup.qml
grep -n "visible.*errorText" package/contents/ui/ProviderGroup.qml
```

Expected:
- `border.color` 只出现一次（switch 表达式那个）
- `visible` 行为 `groupRoot.errorText.length > 0`（无 `&& plans.length === 0`）

- [ ] **Step 4: 提交**

```bash
git add package/contents/ui/ProviderGroup.qml
git commit -m "fix: remove duplicate border.color and fix error label visibility in ProviderGroup"
```

---

### Task 3: 修改 main.qml — 引入 mockData、添加 Timer、修复 tightestProviderName

**Files:**
- Modify: `package/contents/ui/main.qml:1-6` (添加 import)
- Modify: `package/contents/ui/main.qml:10-68` (替换内联数据为 mockData 引用)
- Modify: `package/contents/ui/main.qml:82-94` (修复 tightestProviderName 使用 stripProviderSuffix)
- Add after line 94: Timer 组件

**Interfaces:**
- Consumes: `MockData.SEED_PROVIDERS`、`MockData.stripProviderSuffix()`、`MockData.fluctuateProviders()` (from Task 1)
- Produces: `root.providers` 属性由 mockData 驱动，Timer 每 60s 刷新

- [ ] **Step 1: 添加 mockData import**

在 main.qml 顶部 import 区（第 5 行 `import org.kde.plasma.plasmoid` 之后）添加：

```qml
import "js/mockData.js" as MockData
```

注意：QML 相对路径 import 基于当前 QML 文件位置（`package/contents/ui/`），所以 `js/mockData.js` 解析为 `package/contents/js/mockData.js`。

- [ ] **Step 2: 替换内联 providers 数据为 mockData 引用**

将第 10-68 行的内联 `property var providers: [...]` 替换为：

```qml
    property var providers: MockData.SEED_PROVIDERS
```

- [ ] **Step 3: 修复 tightestProviderName() 使用 stripProviderSuffix**

将第 82-94 行的 `tightestProviderName()` 函数修改为：

```qml
    function tightestProviderName() {
        let worst = -1;
        let name = "";
        for (const p of providers) {
            for (const plan of p.plans) {
                if (typeof plan.usedPercent === "number" && plan.usedPercent > worst) {
                    worst = plan.usedPercent;
                    name = p.providerName;
                }
            }
        }
        return MockData.stripProviderSuffix(name);
    }
```

唯一变化：最后一行 `return name` → `return MockData.stripProviderSuffix(name)`。

- [ ] **Step 4: 添加 Timer 组件**

在 `tightestProviderName()` 函数之后、`openConfig()` 函数之前（约第 95 行位置）插入：

```qml
    // 定时刷新：每 60 秒用波动数据更新 providers
    Timer {
        interval: 60000
        running: true
        repeat: true
        onTriggered: root.providers = MockData.fluctuateProviders(root.providers)
    }
```

- [ ] **Step 5: 验证修改后的 main.qml 语法**

```bash
grep -n "import.*mockData" package/contents/ui/main.qml
grep -n "MockData.SEED_PROVIDERS" package/contents/ui/main.qml
grep -n "stripProviderSuffix" package/contents/ui/main.qml
grep -n "Timer {" package/contents/ui/main.qml
```

Expected: 4 行输出，分别对应上述 4 处修改

- [ ] **Step 6: 提交**

```bash
git add package/contents/ui/main.qml
git commit -m "feat: integrate mockData with Timer refresh and fix tightestProviderName suffix stripping"
```

---

### Task 4: 检查 configGeneral.qml 无效 import 并运行 qmllint

**Files:**
- Check: `package/contents/ui/configGeneral.qml`
- All QML files: qmllint 静态检查

**Interfaces:**
- Consumes: 无
- Produces: qmllint 无错误通过

- [ ] **Step 1: 检查 configGeneral.qml**

当前 configGeneral.qml 内容：
```qml
import QtQuick
import org.kde.plasma.components as PlasmaComponents

PlasmaComponents.ConfigPage {
}
```

`QtQuick` import 在空 ConfigPage 中未被使用，但 QML 引擎不报错（未使用的 import 是警告而非错误）。设计文档要求"移除无效 import"，但 PlasmaComponents.ConfigPage 可能隐式依赖 QtQuick。保守做法：保留不动，让 qmllint 判断。

- [ ] **Step 2: 运行 qmllint 检查所有 QML 文件**

```bash
qmllint package/contents/ui/*.qml 2>&1 || true
```

如果 `qmllint` 未安装：
```bash
which qmllint || echo "qmllint not found, install with: sudo pacman -S qt6-declarative-dev"
```

- [ ] **Step 3: 修复 qmllint 报告的错误（如有）**

根据 qmllint 输出修复。常见问题：
- 未使用的 import → 移除
- 重复属性赋值 → 已在 Task 2 修复
- 属性类型不匹配 → 调整

- [ ] **Step 4: 提交（如有修复）**

```bash
git add -u
git commit -m "fix: resolve qmllint errors across QML files"
```

若无修复则跳过此步。

---

### Task 5: 更新 README.md 添加开发指令

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: 无
- Produces: 开发者可按文档运行 `plasmawindowed` 验证

- [ ] **Step 1: 读取当前 README.md**

```bash
cat README.md
```

- [ ] **Step 2: 在 README.md 适当位置添加开发/验证章节**

在 README 末尾或现有"开发"章节中追加：

```markdown
## 开发与验证

### 前置条件

- KDE Plasma 6 桌面环境
- `plasmawindowed` 命令（通常随 `plasma-desktop` 安装）

### 运行小部件

```bash
# 从项目根目录安装到本地 Plasma 小部件目录
cp -r package ~/.local/share/plasma/plasmoids/org.kde.plasma.aiUsageWatcher/

# 用 plasmawindowed 独立窗口运行（无需添加到面板）
plasmawindowed aiUsageWatcher
```

### 验证清单

1. 圆球显示最紧张供应商的已用百分比
2. 点击圆球展开完整视图，所有供应商卡片正常渲染
3. 等待 60 秒，Timer 触发数据刷新，数值波动
4. 颜色语义：≤5% 红色、≤15% 黄色、>15% 绿色
5. 错误状态：供应商有 errorText 时显示错误信息
```

- [ ] **Step 3: 提交**

```bash
git add README.md
git commit -m "docs: add plasmawindowed development and verification instructions"
```

---

### Task 6: 端到端验证 — plasmawindowed 运行测试

**Files:**
- 无文件修改，纯验证

**Interfaces:**
- Consumes: Task 1-5 的全部产出
- Produces: 验证通过确认

- [ ] **Step 1: 安装小部件到本地 Plasma 目录**

```bash
mkdir -p ~/.local/share/plasma/plasmoids/org.kde.plasma.aiUsageWatcher/
cp -r package/* ~/.local/share/plasma/plasmoids/org.kde.plasma.aiUsageWatcher/
```

- [ ] **Step 2: 运行 plasmawindowed**

```bash
plasmawindowed aiUsageWatcher 2>&1 &
```

注意：需要图形环境。在 SSH 或无显示环境下此步会失败，需在桌面终端执行。

- [ ] **Step 3: 人工验证清单**

按以下顺序检查：

| # | 检查项 | 预期结果 | 通过 |
|---|--------|----------|------|
| 1 | 圆球显示 | 显示百分比数字和颜色环 | ☐ |
| 2 | 点击圆球展开 | 完整视图出现，3 个供应商卡片 | ☐ |
| 3 | 云之声Token Hub 卡片 | 3 条 PlanBar（5小时/7天/30天） | ☐ |
| 4 | MiniMax 卡片 | 1 条 PlanBar（余额 88%），名称显示 "MiniMax"（无后缀） | ☐ |
| 5 | Codex 卡片 | 1 条 PlanBar（周限额 67%），显示 "503/750 次" | ☐ |
| 6 | 等待 60 秒 | 数值波动（±5%），颜色可能变化 | ☐ |
| 7 | 颜色语义 | ≤5% 红、≤15% 黄、>15% 绿 | ☐ |
| 8 | 错误标签 | 有 errorText 的供应商显示错误信息（即使 plans 非空） | ☐ |

- [ ] **Step 4: 记录验证结果**

如果所有检查项通过，在 git 中打标签标记验证完成：

```bash
git tag -a v0.1.0-mvp -m "MVP verified: mock data + timer + bug fixes"
```

如有失败项，记录失败原因并回到对应 Task 修复。

### Task 7: 主题化图表 + 右键菜单样式切换

**背景**：设计文档新增"图表与图标样式规范（主题跟随）"节和"右键菜单样式切换"节。本任务把这 2 条规范落地为代码。

**变更范围**：

| 文件 | 操作 | 说明 |
| ------ | ------ | ------ |
| `package/contents/config/main.xml` | 修改 | `ui` 组新增 `displayStyle`（默认 `"pie"`） |
| `package/contents/ui/main.qml` | 修改 | import `org.kde.quickcharts`，`compactRepresentation` 用 `PieChartControl`，`fullRepresentation` 的 PlanBar 用 `BarChartControl`，颜色全部改 `PlasmaCore.Theme.*`，`contextualActions` 加"打开系统监视器"+"显示样式"子菜单 |
| `package/contents/ui/PlanBar.qml` | 修改 | 改用 `BarChartControl` 替换 Rectangle 宽度动画 |

**步骤**：

- [ ] **Step 1: 在 config/main.xml 加 `displayStyle`**

```xml
<entry name="displayStyle" type="string">
    <default>pie</default>
</entry>
```

- [ ] **Step 2: main.qml 顶部加 import**

```qml
import org.kde.quickcharts 1.0
```

- [ ] **Step 3: main.qml 所有硬编码颜色替换为 `PlasmaCore.Theme` 取色**

  - `#34d399` → `PlasmaCore.Theme.PositiveText`
  - `#fbbf24` → `PlasmaCore.Theme.NeutralText`（黄用 Highlight 或自定义）
  - `#f87171` → `PlasmaCore.Theme.NegativeText`
  - `#6b7280` / `#9ca3af` → `PlasmaCore.Theme.NeutralText`
  - 背景 `Qt.rgba(0.04, 0.05, 0.1, 0.85)` 等深色硬编码 → 改用 `PlasmaCore.Theme.backgroundColor` 加透明度

- [ ] **Step 4: compact 圆球嵌入 PieChartControl**

```qml
PieChartControl {
    anchors.fill: parent
    chartData: [{ "label": i18n("已用"), "value": worst }, { "label": i18n("剩余"), "value": 100 - worst }]
}
```

- [ ] **Step 5: full 面板 PlanBar 用 BarChartControl 替换 Rectangle**

  - 拆 PlanBar.qml 或直接在 main.qml 内联,推荐拆出独立 `PlanBarChart.qml` 复用

- [ ] **Step 6: contextualActions 加"显示样式"子菜单**

```qml
Plasmoid.contextualActions: [
    PlasmaCore.Action { text: i18n("打开系统监视器…"); icon.name: "utilities-system-monitor"; onTriggered: Qt.openUrlExternally("plasma-systemmonitor") },
    PlasmaCore.Action { text: i18n("配置…"); icon.name: "configure"; onTriggered: plasmoid.action("configure").trigger() },
    PlasmaCore.Action {
        text: i18n("显示样式")
        icon.name: "preferences-desktop-display"
        PlasmaCore.Action {
            text: i18n("饼状图"); icon.name: "office-chart-pie"
            checkable: true; autoExclusive: true
            checked: plasmoid.configuration.displayStyle === "pie"
            onTriggered: plasmoid.configuration.displayStyle = "pie"
        }
        // 柱状图 / 传感器详情 同模式
    },
    PlasmaCore.Action { text: i18n("刷新"); icon.name: "view-refresh"; onTriggered: root.providers = MockData.fluctuateProviders(root.providers) }
]
```

- [ ] **Step 7: 同步到本地 plasmoid 目录,plasmawindowed 启动验证三种样式都能切换**

```bash
rsync -a package/contents/ ~/.local/share/plasma/plasmoids/aiUsageWatcher/contents/
pkill -f "plasmawindowed aiUsageWatcher"
nohup plasmawindowed aiUsageWatcher >/tmp/pw.out 2>&1 &
```

- [ ] **Step 8: 验证 qmllint + 视觉检查**

```bash
qmllint package/contents/ui/*.qml
```

- [ ] **Step 9: 提交**

```bash
git add docs/superpowers package/contents
git commit -m "feat: theme-aware charts (PieChartControl/BarChartControl) + right-click display style menu"
```

**风险**：

- `org.kde.quickcharts` 可能未在所有发行版自带 — Manjaro 上 `/usr/lib/qt6/qml/org/kde/quickcharts/controls/PieChartControl.qml` 真实存在,Plasma 6 默认带
- 切样式时圆球/PlanBar 重绘可能闪一下,用 `Behavior` 缓 200ms 改善

---

## Self-Review

### 1. Spec Coverage

| 设计文档要求 | 对应 Task | 状态 |
|-------------|-----------|------|
| 创建 mockData.js + SEED_PROVIDERS | Task 1 | 覆盖 |
| stripProviderSuffix() | Task 1 + Task 3 | 覆盖 |
| fluctuateProviders() | Task 1 | 覆盖 |
| Timer 60s 刷新 | Task 3 | 覆盖 |
| 修复重复 border.color | Task 2 | 覆盖 |
| 修复错误标签可见性 | Task 2 | 覆盖 |
| tightestProviderName 去后缀 | Task 3 | 覆盖 |
| Orb 灰色态处理 | — | 已在现有代码中实现（main.qml 第 119-133 行），无需修改 |
| 移除 configGeneral 无效 import | Task 4 | 覆盖（检查后决定） |
| qmllint 无错误 | Task 4 | 覆盖 |
| README 开发指令 | Task 5 | 覆盖 |
| plasmawindowed 验证 | Task 6 | 覆盖 |

### 2. Placeholder Scan

无 TBD/TODO/实现稍后等占位符。所有步骤包含完整代码或明确命令。

### 3. Type Consistency

- `MockData.SEED_PROVIDERS` 类型为 `var`（QML JS 数组），与 `root.providers: var` 一致
- `stripProviderSuffix(name: string) → string`，在 `tightestProviderName()` 中调用，返回值赋给 QML 绑定
- `fluctuateProviders(providers: Provider[]) → Provider[]`，返回新数组，直接赋值 `root.providers` 触发绑定刷新
- ProviderGroup 的 `plans` 属性类型为 `var`，与 mockData 返回的 plans 数组一致
- PlanBar 的各属性（planName, usedPercent, usedPercentLabel, barClass 等）与 mockData plan 对象字段名完全对齐
