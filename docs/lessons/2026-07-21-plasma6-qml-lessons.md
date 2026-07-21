# 经验教训

> 项目开发过程中记录的经验教训，避免重复踩坑。

## 2026-07-21 — minimal-viable-plasmoid 构建阶段

### 1. QML + Plasma 6 兼容性

**Plasma 6 API 与 Plasma 5 不兼容，不能照搬旧代码。**

| 问题 | 错误信息 | 根因 | 正确做法 |
|------|---------|------|---------|
| `Plasmoid.compactRepresentation` | "Cannot assign to non-existent property" | Plasma 5 语法，Plasma 6 中 `compactRepresentation` 是 `PlasmoidItem` 的直接属性 | 直接写 `compactRepresentation: MouseArea { ... }` |
| `ScrollView` | "ScrollView is not a type" | Plasma 6 原生 QML 不包含 `ScrollView` | 改用 `Flickable` 或 `ListView` |
| `Label` | "Label is not a type" | 裸 `Label` 不是 Plasma 6 内置类型 | 用 `PlasmaComponents.Label` |
| `PlasmaCore.Action` | "Action is not a type" | `contextualActions` 接受 `QAction`，但需要正确 import | 用 `PlasmaCore.Action` + `import org.kde.plasma.core as PlasmaCore` |
| JS import 路径 | "Script file ... unavailable" | QML 的 `import` 路径是相对于当前 QML 文件 | `main.qml` 在 `contents/ui/`，`mockData.js` 在 `contents/js/`，所以写 `import "../js/mockData.js"` |

### 2. 布局与渲染

**`ColumnLayout` 嵌套容易导致高度计算错误，出现重叠或空白。**

- 内部 Row 使用固定宽度 `width: parent.width - X` 比 `Layout.fillWidth: true` 更可靠，不会在嵌套 Layout 中撑爆
- `Flickable` + `Column` 组合中，`contentHeight` 必须绑定 `column.height`（不是 `implicitHeight`），否则滚动区域计算错误
- `ListView` 最适合动态列表渲染，自动计算 item 高度，不需要手动维护 `contentHeight`

### 3. 种子数据与逻辑函数的一致性

**种子数据中的人工赋值不能与程序逻辑函数冲突。**

`MiniMax` 的 `usedPercent: 88` 但人工设为 `barClass: "bar-yellow"`，而 `_barClass()` 函数规定 `> 15% = green`。第一次 Timer 触发波动后，`_barClass()` 会重新计算，导致颜色从黄色跳到绿色，视觉不一致。

**教训：** 种子数据只设 `usedPercent`，颜色/样式类由函数自动计算。如果要测试特定颜色状态，设对应的 `usedPercent` 值而不是人工硬编码样式类。

### 4. Subagent 驱动开发的适用性

**Subagent 驱动适合逻辑实现，不适合纯 UI 调试。**

| 任务类型 | 适合 Subagent | 原因 |
|---------|--------------|------|
| 数据模块 / JS 逻辑 | ✅ | 有明确输入输出，subagent 可独立验证 |
| Bug 修复（语法级） | ✅ | 修改位置明确，`qmllint` 可验证 |
| UI 布局调整 | ❌ | 需要视觉反馈循环，subagent 看不到运行效果 |

**改进：** UI 布局调整应该在本地运行 `plasmawindowed` 看效果，确认后再提交，而不是 subagent 提交 → 主会话安装 → 截图 → 反馈循环。

### 5. TDD 的适用边界

**QML 组件测试成本高，UI 层不适合强制 TDD。**

- `tdd_mode: tdd` 在本项目中导致 implementer 把时间花在写测试上，而 QML 的测试框架（`qmltest`）需要额外配置且不常用
- 对于纯 QML 前端项目，`tdd_mode: direct` + `review_mode: standard` 更合适

### 6. 组件化

**`main.qml` 膨胀到 360 行，应当解耦。**

- `ProviderGroup` / `PlanBar` / `Orb` 组件已存在但未被复用（因为内联写了布局）
- 下次重构时应当把 `ListView` 的 delegate 提取为独立组件，便于单文件维护和 qmllint 检查

### 7. Comet 流程体感

| 阶段 | 体验 | 建议 |
|------|------|------|
| open | 清晰，三件套（proposal/design/tasks）结构好 | 保持 |
| design | brainstorming + Design Doc 交付物可追溯 | 保持 |
| build | 阶段最长，UI 调试不适合 TDD | UI 任务用 `direct` 模式 |

**Handoff hash 校验：** design 阶段 handoff 生成后，如果修改了 spec 文件（如 brainstorming 中补充种子数据），必须重新运行 `comet-handoff.sh` 更新 hash，否则 guard 会失败。这不是 bug，而是防止产物漂移的机制。