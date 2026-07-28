# 悬浮面板本周用量两段式设计

## 修订说明

本版本取代“按每天记录并持久化历史”的原方案。新方案不保存历史，只使用 CodexZH
当前接口响应，把本周已使用部分实时拆成：

1. 此前使用：本周已使用减去今日使用。
2. 今日使用：接口返回的今日消费。

## 目标

CodexZH 周限额在展开后的悬浮面板中显示两个连续颜色段，让用户区分今天和此前的
额度消耗。两个颜色段相加必须等于本周总已使用部分。

顶部 Plasma 面板的小图标保持当前单色显示。

## 成功标准

- 仅 CodexZH 周限额启用两段式显示。
- 此前使用金额与今日使用金额之和等于 `weekUsed`。
- 展开面板的列表模式和饼图模式使用相同的两段数据。
- 鼠标悬浮颜色段时显示该段名称、百分比和金额。
- 今日字段缺失或无效时回退现有单色图表。
- Codex、MiniMax 和其他供应商保持现有单色逻辑。
- `CompactView.qml` 不因本功能发生改动。
- 不新增历史存储、KConfig、后台定时归档或账户隔离逻辑。

## 数据来源

CodexZH 响应中的字段：

| 字段 | 用途 |
|---|---|
| `weekUsed` | 本周累计使用金额 |
| `weeklyBudget` / `weeklyQuota` | 本周总额度 |
| `todayUsed` | 今日使用金额的原始数值 |
| `todayUsedFormatted` | 今日金额的接口格式化展示文本 |

数学计算必须使用原始数值 `todayUsed`，在 C++ 中命名为 `todayUsedUsd`。
`todayUsedFormatted` 可能包含货币符号、特殊标记或格式差异，只能交给
`formatUsdFromApi(data.todayUsedFormatted, todayUsedUsd)` 生成展示文本，禁止从该字符串
反解析数值。

## 计算规则

```text
weekUsedUsd     = max(weekUsed, 0)
todayUsedUsd    = clamp(todayUsed, 0, weekUsedUsd)
previousUsedUsd = weekUsedUsd - todayUsedUsd

todayPercent    = todayUsedUsd / weeklyLimit * 100
previousPercent = previousUsedUsd / weeklyLimit * 100
```

金额使用未舍入的数值计算；百分比仅在显示文本时格式化。由此保证：

```text
previousUsedUsd + todayUsedUsd = weekUsedUsd
previousPercent + todayPercent = weekUsedUsd / weeklyLimit * 100
```

边界规则：

- `todayUsed == 0`：只显示“此前使用”段。
- `previousUsed == 0`：只显示“今日使用”段。
- 两者都为 0：只显示未使用轨道。
- `todayUsed > weekUsed`：按接口数据不一致处理，将今日金额钳制为 `weekUsed`，此前为 0。
- `todayUsed` 缺失、非数字、非有限数或小于 0：不生成两段数据，回退现有单色图表。
- `weeklyLimit <= 0` 或 `weekUsed` 无效：沿用现有无效快照处理。

## 数据模型

CodexZH 周限额快照增加可选的结构化段：

```text
usageSegments: [
  {
    kind: "previous",
    label: "此前使用",
    used: 51.0,
    usedPercent: 20.0,
    formattedUsed: "$51.00"
  },
  {
    kind: "today",
    label: "今日使用",
    used: 76.5,
    usedPercent: 30.0,
    formattedUsed: "$76.50"
  }
]
```

- 数值为 0 的段不进入数组。
- 数组顺序固定为“此前使用”在前、“今日使用”在后。
- `usageSegments` 不存在时，QML 走当前单色路径。
- C++ 后端生成并校验结构化段；两个 QML 图表只消费数据，不重复计算业务规则。
- `displayProvider.js` 只验证并透传数组，不从 `extraText` 提取数据。
- 展开面板的填充几何使用各段未取整 `usedPercent` 之和；整体百分比文字继续沿用现有
  取整后的 `usedPercentLabel`。两者只存在显示精度差异，不改变金额或额度语义。

## 展示范围

### 顶部面板

`CompactView.qml` 继续只读取整体 `usedPercent`，保留当前单色圆环或单色水平条，不显示
两段颜色，也不增加分段提示。

### 展开面板列表模式

`PlanBar.qml` 继续使用 `QQC2.ProgressBar`：

- 有 `usageSegments` 时，`value` 为各段精确百分比之和；无分段时仍为现有整体
  `usedPercent`。
- 有 `usageSegments` 时，`contentItem` 内依次绘制此前段和今日段。
- 无结构化段时保留当前单色填充。
- 未使用部分继续由现有底轨显示。

### 展开面板饼图模式

`PanelPieView.qml` 继续使用 `Charts.PieChart`：

- 有 `usageSegments` 时，用 `Charts.ArraySource` 提供两个百分比值和对应颜色。
- 图表范围固定为 `0..100`，剩余额度继续由 `backgroundColor` 绘制。
- 无结构化段时保留当前 `SingleValueSource` 单色路径。

## 颜色与提示

- “此前使用”和“今日使用”使用两种固定、主题适配且容易区分的颜色。
- 同一段在列表模式和饼图模式下颜色一致。
- 颜色不再表示额度紧张程度；整体百分比文字仍可沿用现有语义色。
- Plasma 原生 ToolTip 文本：
  - `此前使用 · 20% · $51.00`
  - `今日使用 · 30% · $76.50`
- 百分比最多显示两位小数，整数不显示无意义的小数位。
- 图表的无障碍描述同时包含两段名称、百分比和金额，不能只靠颜色区分。

水平条可直接为每个 QML 段绑定 `HoverHandler` 和 Plasma ToolTip。

KQuickCharts 没有公开当前悬浮扇区索引。饼图使用一个 Qt `HoverHandler` 获取坐标，
通过圆环内外半径和累计角度判断当前段；命中中心、图表外部或未使用区域时不显示提示。
该计算只负责交互命中，不参与图表绘制。

## 数据流

```text
CodexZH response
  ├─ weekUsed ──────────────┐
  ├─ weeklyLimit ───────────┼─> CodexZH C++ backend
  ├─ todayUsed ─────────────┤       ├─ overall usedPercent
  └─ todayUsedFormatted ────┘       └─ optional usageSegments[previous, today]
                                             │
                                   displayProvider passthrough
                                      ┌──────┴────────┐
                                      v               v
                                   PlanBar       PanelPieView

CompactView <────────────── overall usedPercent only
```

## 错误与降级

| 情况 | 行为 |
|---|---|
| 非 CodexZH 供应商 | 保持当前单色 |
| 今日原始数值缺失或无效 | 保持当前单色，不解析格式化字符串 |
| 今日金额为 0 | 只显示此前段 |
| 此前金额为 0 | 只显示今日段 |
| 今日金额大于本周金额 | 今日钳制为本周金额，此前为 0 |
| 两段数组不存在或结构无效 | QML 保持当前单色 |
| 悬浮在未使用区域 | 不显示分段提示 |

任何两段计算错误都不得改变整体 `weekUsed`、总百分比和现有详情文本。

## 测试

### C++ 解析与映射

- `weekUsed=50`、`todayUsed=30`：此前 20、今日 30，两段合计 50。
- 带小数的两段百分比之和等于未取整的本周使用率，整体标签仍按现有规则取整。
- `todayUsed=0`：仅此前段。
- `todayUsed=weekUsed`：仅今日段。
- `weekUsed=0`、`todayUsed=0`：空段数组。
- `todayUsed > weekUsed`：今日钳制为本周金额。
- 今日字段缺失、字符串、NaN、无穷或负数：不生成两段，整体周用量仍按现有规则处理。
- `todayUsedFormatted` 优先用于今日金额展示，原始数值作为回退。

### 展示模型

- 有效 `usageSegments` 原样透传。
- 不存在、非数组或包含无效数值时不进入分段路径。
- 非 CodexZH 快照不因计划名称包含“周”而自动启用分段。

### QML

- `PlanBar` 两段宽度之和等于整体已用宽度，顺序为此前、今日。
- `PlanBar` 的两段悬浮提示内容正确；0 值段不创建。
- `PanelPieView` 的值源、颜色源、范围和剩余轨道正确。
- 饼图命中此前段、今日段、中心、外部和未使用区域的结果正确。
- 缺少结构化段时，两个展开面板图表继续使用当前单色。
- `CompactView` 继续忽略 `usageSegments`，现有 compact 行为不变。

### 验证门

- 运行相关 C++ 单元测试和 QML 测试。
- 运行 `tests/run-static-checks.sh`。
- 运行安装级 Plasma smoke，人工验证列表/饼图的两段颜色和提示。
- 检查 diff，确认 `CompactView.qml` 没有因本功能发生改动。

## 不在范围

- 不记录周内每天的历史。
- 不推导具体日期或补算漏采天数。
- 不读写 KConfig 历史。
- 不区分多个账户的历史。
- 不扩展 Codex、MiniMax 或自定义供应商。
- 不增加历史页面、导出、颜色设置或每日预算线。
- 不改变顶部面板小图标。
- 不引入 Canvas、QtQuick Shapes、自定义 Shader 或第三方图表库。
