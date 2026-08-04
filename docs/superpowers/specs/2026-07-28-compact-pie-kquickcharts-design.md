# 图表原生绘制改造设计

## 目标

项目内所有额度图表统一使用 KDE/Qt 原生组件，改善 Plasma 面板小尺寸下的抗锯齿效果：

- 饼图统一使用 Plasma CPU/内存监控同款
  `org.kde.ksysguard.piechart` → `org.kde.quickcharts` 的 `Charts.PieChart` 绘制链。
- 水平用量条统一使用 Plasma 硬盘使用率监控同款
  `org.kde.ksysguard.horizontalbars` → `QQC2.ProgressBar` 绘制结构，并完全覆写
  原生控件的内容和背景。
- 仅数据来源仍为本项目的额度快照；图表组件和渲染技术与上述 Plasma 原生监控一致。

## 范围

- 修改 `package/contents/ui/CompactView.qml` 的饼图和水平用量条。
- 修改 `package/contents/ui/PlanBar.qml` 的水平用量条。
- 核对 `package/contents/ui/PanelPieView.qml`；它已经使用 `Charts.PieChart`，无需重写。
- 更新 `tests/tst_compactView.qml` 和 `tests/tst_providerGroup.qml` 的图表行为测试。
- 不修改数据模型、配置格式、百分比/颜色语义和面板布局。

## 现有能力

- `PanelPieView.qml` 已用 `Charts.PieChart + Charts.SingleValueSource`、固定 `0..100`
  范围和 `backgroundColor` 绘制圆环，可直接复用。
- `CompactView.qml` 与 `PlanBar.qml` 已有百分比钳制、语义色和底轨；水平用量条保留
  300ms 动画。
- 项目运行环境已经依赖 KQuickCharts，不新增第三方依赖或安装步骤。

## 组件映射

```text
currentUsage.usedPercent
          │
          ├─ clamp 0..100 ──> Charts.SingleValueSource
          │                         │
          │                  Charts.PieChart
          │                  ├─ value arc: usageColor()
          │                  └─ background: trackColor
          │
          └─ clamp 0..100 ──> QQC2.ProgressBar.value
                                    │
                            contentItem Item（占满可用宽度）
                                    │
                            └─ fill Rectangle
                               width = contentItem.width × visualPosition
                            background Rectangle（完整底轨）
```

| 展示位置 | 图表 | 原生组件 | 处理 |
|---|---|---|---|
| compact | 环形饼图 | `Charts.PieChart` | 替换 `Shape/PathAngleArc` |
| compact | 水平用量条 | `QQC2.ProgressBar` | 替换自定义 `Item` 容器 |
| popup | 环形饼图 | `Charts.PieChart` | 已符合，仅核对 |
| popup | 水平用量条 | `QQC2.ProgressBar` | 替换 `PlanBar` 自定义 `Item` 容器 |

## 实现约束

### 饼图

- compact 使用一个 `Charts.SingleValueSource` 提供已使用百分比，范围固定为 `0..100`。
- 已使用圆弧沿用 `usageColor()`；剩余轨道使用 `backgroundColor`。
- `smoothEnds: true`，厚度与当前视觉尺寸一致。
- 数据源直接绑定钳制后的百分比，跟随 Plasma 原生 `Charts.PieChart` 更新方式；不增加
  自定义动画代理。
- `usedPercent < 0` 时值为 `0`、颜色为禁用色，中心文字继续显示 `—`。

### 水平用量条

- compact 和 popup 均使用 `QQC2.ProgressBar`，`from: 0`、`to: 100`，
  `value` 为钳制后的已使用百分比。
- 像 KDE KSysGuard 上游一样完全覆写 `contentItem` 和 `background`，内部使用圆角
  `Rectangle`；不依赖 Breeze 的默认 ProgressBar 主题渲染。
- `contentItem` 必须是占满控件可用宽度的容器；填充 `Rectangle` 是其子项，宽度为
  `contentItem.width * control.visualPosition`。不得把 `contentItem` 自身宽度绑定到
  `visualPosition`，以免形成自引用或丢失完整布局宽度。
- 填充子项的宽度保留 300ms `Easing.OutCubic` 动画。
- 轨道在 `0%` 和无数据时仍可见；无数据文字和语义色保持现状。
- 保留现有可访问名称和描述。

### 测试契约

- 保留面向功能的根对象名；删除 `Shape` 专属的 `compactPieTrack/compactPieArc`
  内部契约，改为验证 `Charts.PieChart` 的数据源、范围和可见性。
- 为饼图颜色源保留稳定对象名，验证正常值使用现有语义色、无数据使用禁用色；不做
  像素级颜色测试。
- 为 compact 和 popup 的水平填充保留稳定对象名，直接验证其颜色绑定；无数据测试
  同步提供 `usedPercent: -1` 与展示标签 `—`，不假定组件负责派生标签。
- 不为保持旧测试而创建不可见占位对象。

## 测试计划

```text
CompactView
├─ pie
│  ├─ 0%       -> 单值源 0，轨道可见，文字 0%
│  ├─ 50%      -> 单值源 50，语义色正确
│  ├─ 100%     -> 单值源 100
│  └─ 无数据   -> 单值源 0，禁用色，文字 —
└─ bar
   ├─ 0/50/100 -> ProgressBar value 正确且范围为 0..100
   └─ 无数据   -> value 0，轨道仍可见

PlanBar
├─ 0/50/100    -> ProgressBar value 正确、底轨存在
└─ 无数据      -> value 0、文字保持 —

Plasma smoke
└─ compact/popup × pie/bar × 亮/暗主题 × 100%/125%/150% 缩放
   -> 无 QML 加载错误、无明显锯齿、无裁切或空白图表
```

- QML 行为测试：`tests/tst_compactView.qml`、`tests/tst_providerGroup.qml`。
- 自动门：`bash tests/run-static-checks.sh`。静态检查覆盖现有全部图表文件：
  `CompactView.qml` 必须使用 `Charts.PieChart + QQC2.ProgressBar`，
  `PanelPieView.qml` 必须使用 `Charts.PieChart`，`PlanBar.qml` 必须使用
  `QQC2.ProgressBar`；三者均禁止回退到自绘图表技术。
- 安装级门：`bash tests/run-plasma-smoke.sh`。缺少可见 Plasma 会话时可以记录
  `BLOCKED`，但只能标记“代码完成、视觉待验”，不得宣称本次视觉改造完成。
- 人工视觉门：在真实 Plasma 面板尺寸下保存 compact 与 popup 两种图表截图，
  核对亮暗主题及 100%/125%/150% 缩放；该门通过后才能完成本次改造。
- 当前工作区存在与本改造无关的旧 `mockData.js` 测试引用；实施前先记录测试基线，
  若全量门因此失败，须单独报告为既有问题，并继续运行本次直接相关的 QML 测试，
  不得把既有失败误归因于图表改造或把未通过说成通过。相关 QML 测试与生产 QML
  `qmllint` 必须通过；全量套件仍为红时，只能报告“范围内验证通过、仓库基线未通过”。

## 失败模式

| 失败模式 | 防护 |
|---|---|
| KQuickCharts 导入或类型加载失败 | qmllint、QML Test 和 Plasma smoke 阻止交付 |
| 无数据被误绘制为有效 0% | 值为 0，但禁用色和中心 `—` 保留无数据语义 |
| ProgressBar 恢复 Breeze 不可见问题 | 完全覆写 `contentItem/background`，并验证底轨尺寸 |
| 100% 圆弧端点或小尺寸出现缝隙 | `smoothEnds`、固定范围和真实面板截图验收 |
| 重构后旧内部对象名导致脆弱测试 | 测试改验原生组件的值、范围与可见行为 |

## 性能

KQuickCharts 通过 Qt Quick Scene Graph 和 GPU SDF 着色器绘制圆环；每个图表只有常量级
节点和数据源。ProgressBar 仍只有背景与填充两个 Rectangle。不启用额外离屏图层、超采样
或缓存。

## 实施顺序

1. 先更新 `tst_compactView.qml` 与 `tst_providerGroup.qml`，覆盖原生组件和边界值。
2. 替换 `CompactView.qml` 的饼图与水平用量条。
3. 替换 `PlanBar.qml` 的水平用量条，并核对 `PanelPieView.qml`。
4. 运行静态检查、构建、Plasma smoke 和人工缩放截图验收。

Sequential implementation, no parallelization opportunity：核心改动集中在同一组 QML 图表组件，
并共享测试与对象契约；项目规则也禁止创建 git worktree。

## NOT in scope

- 不更改 compact/popup 布局、尺寸选项或图表类型配置。
- 不更改百分比计算、供应商轮询、颜色阈值和错误状态。
- 不新增自定义 Shader、Canvas、SVG、离屏超采样或图表抽象层。
- 不重构与图表绘制无关的 Provider、KCM 和原生查询后端。

## Implementation Tasks

- [ ] **T1 (P1, human: ~1h / Codex: ~10min)** — 测试 — 补齐原生图表的 0/50/100/无数据行为测试
  - Surfaced by: Test Review — 现有测试只检查旧 Shape 对象和基本可见性。
  - Files: `tests/tst_compactView.qml`, `tests/tst_providerGroup.qml`
  - Verify: `bash tests/run-static-checks.sh`
- [ ] **T2 (P1, human: ~1h / Codex: ~10min)** — compact — 使用 KQuickCharts 与 ProgressBar 替换自定义图表
  - Surfaced by: Architecture Review — 统一使用项目现有 KDE 原生组件。
  - Files: `package/contents/ui/CompactView.qml`
  - Verify: `bash tests/run-static-checks.sh`
- [ ] **T3 (P1, human: ~30min / Codex: ~5min)** — popup — 使用 ProgressBar 替换 PlanBar 自定义水平条
  - Surfaced by: Architecture Review — 用户要求项目全部水平图表使用原生组件。
  - Files: `package/contents/ui/PlanBar.qml`（`PanelPieView.qml` 只审计，不改动）
  - Verify: `bash tests/run-static-checks.sh`
- [ ] **T4 (P1, human: ~45min / Codex: ~10min)** — QA — 验证真实 Plasma 尺寸与缩放下的抗锯齿效果
  - Surfaced by: Test Review — 逻辑测试不能证明 GPU 输出的边缘质量。
  - Files: `tests/README.md`, `tests/run-static-checks.sh`（仅做现有验收门所需的最小修改）
  - Verify: `bash tests/run-plasma-smoke.sh` 并保存人工截图；无可见 Plasma 会话时保持未完成

## GSTACK REVIEW REPORT

| Review | Trigger | Why | Runs | Status | Findings |
|--------|---------|-----|------|--------|----------|
| CEO Review | `/plan-ceo-review` | Scope & strategy | 0 | — | 本次不改变产品范围 |
| Codex Review | `/codex review` | Independent 2nd opinion | 2 | CLEAR (fallback) | CLI 受权限策略阻断；独立只读审查的 3 项验收缺口已折叠 |
| Eng Review | `/plan-eng-review` | Architecture & tests (required) | 2 | CLEAR | 6 项设计缺口已写入方案，0 个关键缺口 |
| Design Review | `/plan-design-review` | UI/UX gaps | 0 | — | 原生同款视觉由真实 Plasma 截图门验收 |
| DX Review | `/plan-devex-review` | Developer experience gaps | 0 | — | 不适用 |

**CODEX:** CLI 复核受权限策略阻断；独立回退审查补齐了全局图表约束、颜色测试契约和测试基线表述。

**VERDICT:** ENG CLEARED — 方案可进入实施；真实 Plasma 视觉门通过后才能宣称改造完成。

NO UNRESOLVED DECISIONS
