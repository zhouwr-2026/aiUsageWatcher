# 悬浮面板每日用量分段设计

## 目标

对能够返回“今日用量”的周套餐，在展开后的悬浮面板中把本周已用额度按实际使用日期拆成连续彩色段，并在鼠标悬浮时显示对应日期和用量占周额度的百分比。

示例：周一使用周额度的 20%，周二未使用，周三使用 30%，图表的已用 50% 由周一和周三两段颜色连续组成；悬浮第二段显示“7月29日 · 使用 30%”。

## 成功标准

- 只有 CodexZH 周限额启用每日分段。
- Codex、MiniMax 及其他没有今日用量字段的供应商保持当前单色图表。
- 展开面板的列表模式和饼图模式都显示相同的每日分段与提示。
- 用量为 0 的日期不显示空段，非连续日期的有效段仍紧密排列。
- 顶部 Plasma 面板的 `CompactView` 完全保持现状。
- 继续使用 `Charts.PieChart`、`QQC2.ProgressBar`、Qt Pointer Handler 和 Plasma ToolTip，不引入 Canvas、QtQuick Shapes、SVG 图表、自定义 Shader 或第三方图表库。

## 已确认的数据能力

| 供应商 | 周累计数据 | 今日用量 | 结论 |
|---|---|---|---|
| CodexZH | `weekUsed`、`weeklyBudget`/`weeklyQuota` | `todayUsed` | 启用每日分段 |
| Codex | 窗口 `used_percent`、`limit_window_seconds`、`reset_at` | 无 | 保持单色 |
| MiniMax | `current_weekly_remaining_percent`、周开始/结束时间 | 无 | 保持单色 |

CodexZH 的 `todayUsed` 当前只被拼入 `extraText`，实现时需把它作为结构化数值保留在周限额快照中。某次响应缺少或无法解析 `todayUsed` 时，该次快照回退为当前单色图表，不根据字符串提示猜测数值。

## 范围

### 修改

- CodexZH 响应解析和快照映射：输出结构化今日用量。
- CodexZH 客户端：记录本周每日用量并生成展示段。
- 展示模型：把每日段透传到展开面板。
- `PlanBar.qml`：列表模式的水平每日分段和悬浮提示。
- `PanelPieView.qml`：饼图模式的圆环每日分段和悬浮提示。
- 对应的 C++、展示模型和 QML 行为测试。

### 不修改

- `CompactView.qml` 的数据选择、单色圆环、单色水平条和顶部提示。
- Codex、MiniMax 以及自定义供应商的数据协议和图表行为。
- 供应商配置格式、刷新频率、排序和整体用量百分比。
- 现有用量颜色阈值；没有每日段时继续使用现有绿/黄/红语义色。

## 数据模型

CodexZH 周限额计划在现有字段之外增加：

```text
dailySegments: [
  {
    date: "2026-07-27",
    used: 51.0,
    usedPercent: 20.0,
    kind: "day"
  },
  {
    date: "2026-07-29",
    used: 76.5,
    usedPercent: 30.0,
    kind: "day"
  }
]
```

- `used` 保留接口原始额度单位，避免持久化舍入误差。
- `usedPercent = used / weeklyLimit * 100`，展示前钳制到有效范围。
- `date` 使用本地日期的 ISO 格式作为稳定键，提示文本在展示层格式化。
- 颜色由展示顺序决定，不写入持久化数据。
- `dailySegments` 不存在时按旧单色路径渲染；空数组表示已启用但本周尚无用量。

## 后台记录与推导

历史只服务 CodexZH，不建立通用历史框架。CodexZH 客户端使用现有 `aiquotapilotrc` 的独立 KConfig 组保存当前周最多 7 个日期值和周期键。

每次 CodexZH 成功刷新时：

1. 根据本地自然周计算周期键；周期变化时删除上一周期数据。
2. 将当天 `todayUsed` 覆盖写入当天记录。0 也要保存，用于区分“确认未使用”和“没有采样”。
3. 用 `weekUsed - todayUsed - 已知历史日用量` 计算此前尚未归属的余额。
4. 若此前只缺一个日期，把余额精确归入该日期。例如首次在周二运行时，可以直接补出周一。
5. 若缺少两个及以上日期，无法可靠拆分，保留一个 `kind: "unattributed"` 的“本周此前用量”段，不伪造日期。
6. 展示时过滤用量为 0 的日期，并按日期升序排列；日期之间不保留视觉空隙。
7. 仅在记录变化时同步 KConfig，避免每分钟重复写盘。

若同一周期内接口校正导致 `weekUsed` 小于已记录日用量之和，当前接口值优先：放弃冲突的历史拆分，该次显示单色总用量；下一次有效刷新重新开始记录。任何计算结果都不得为负数或超过本周累计用量。

## 展示与交互

### 列表模式：`PlanBar`

- 外层继续使用当前 `QQC2.ProgressBar`，`value` 仍为周累计 `usedPercent`。
- `contentItem` 内按累计百分比放置每日颜色段，未使用部分继续显示现有底轨。
- 分段共享一条连续填充，不为零用量日期创建对象。
- 每段使用 Qt 原生悬浮处理和 Plasma ToolTip。
- 提示格式：
  - 日期段：`M月d日 · 使用 X%`
  - 无法拆分的段：`本周此前用量 · X%`

### 饼图模式：`PanelPieView`

- 继续使用 `Charts.PieChart`，固定范围 `0..100`。
- 有每日段时，`valueSources` 改为每日百分比数组，`colorSource` 使用对应颜色数组；剩余周额度继续由 `backgroundColor` 绘制。
- KQuickCharts 没有公开“当前悬浮分段”信号，因此使用一个 Qt `HoverHandler` 获取图表内坐标：
  1. 先判断指针是否位于圆环内外半径之间。
  2. 把坐标换算成与 `PieChart.fromAngle` 一致的角度。
  3. 用每日段累计百分比定位段索引。
  4. 索引有效时显示 Plasma ToolTip，移出圆环或进入未使用区域时隐藏。
- 角度计算只负责原生图表的交互命中，不参与绘制。

### 颜色

- 每周按展示顺序从一组主题适配的固定颜色中取色，同一日期在列表和饼图模式下颜色一致。
- 颜色只用于区分日期，不表达绿/黄/红的额度紧张程度。
- 日期和百分比同时出现在提示与无障碍描述中，不能只靠颜色传达信息。

## 数据流

```text
CodexZH response
  ├─ weekUsed + weeklyLimit ───────────────> existing usedPercent
  └─ todayUsed
        │
        v
  CodexZhClient current-week KConfig history
        │
        ├─ known daily values
        └─ optional unattributed prior usage
                    │
                    v
             plan.dailySegments
                    │
          displayProvider passthrough
             ┌──────┴────────┐
             v               v
          PlanBar       PanelPieView

CompactView <──────── existing usedPercent only
```

## 错误与降级

| 情况 | 行为 |
|---|---|
| 供应商不是 CodexZH | 走现有单色路径 |
| `todayUsed` 缺失、非数字或为负数 | 当前快照走单色路径，不写历史 |
| `weeklyLimit <= 0` | 保留现有无效数据处理 |
| 当天用量为 0 | 保存零值，但不创建可见段 |
| 连续漏采多个日期 | 合并为“本周此前用量”，不猜测日期 |
| 周期重置 | 删除旧周记录，从新周重新记录 |
| 历史与接口累计值冲突 | 接口值优先，当前回退单色 |
| 悬浮在圆环中心、外部或未使用区域 | 不显示每日提示 |

## 测试

### C++

- CodexZH 解析器正确输出 `todayUsed`、`weekUsed` 和周额度。
- 缺失、非数字、负数的 `todayUsed` 不产生每日段。
- 周一/周二推导、跨零用量日期、周期重置和旧周清理。
- 多个漏采日期生成一个未归属段。
- 接口累计值回退时不输出错误的历史分段。
- 相同快照重复刷新不产生重复段。

### QML

- `PlanBar` 在 `[20, 30]` 时生成两段，宽度合计为 50%，中间没有空白日期段。
- 水平条悬浮第二段显示对应日期和 30%。
- `PanelPieView` 的多值源、颜色源和总范围正确。
- 圆环命中首段、第二段、中心、外部和未使用区域的结果正确。
- 没有 `dailySegments` 时，两个展开面板图表继续使用当前单色。
- Codex、MiniMax 快照即使是周窗口也不生成每日段。
- `CompactView` 仍只读取整体 `usedPercent`；现有 compact 测试无需改变每日分段预期。

### 验证门

- 运行相关 C++ 单元测试和 QML 测试。
- 运行 `tests/run-static-checks.sh`。
- 运行安装级 Plasma smoke，人工检查列表/饼图两种展开面板的分段、提示位置和亮暗主题。
- 检查 diff，确认 `CompactView.qml` 没有因本功能发生改动。

## 风险

- 电脑关闭期间无法获得逐日数据；多个漏采日期只能诚实显示为未归属用量。
- 很小的用量段难以悬浮命中；其视觉宽度和数值保持真实，不人为放大，完整摘要保留在无障碍描述中。
- KQuickCharts 只负责绘制，不提供分段鼠标索引；圆环命中算法必须用边界测试覆盖起始角、100% 边界和圆环内外半径。

## 明确不做

- 不从 Codex 或 MiniMax 的累计百分比猜测今日用量。
- 不为自定义供应商扩展今日用量脚本协议。
- 不增加历史页面、周报、导出、颜色配置或每日预算线。
- 不改变顶部面板小图标。
- 不引入通用图表组件或通用时间序列存储。

