## Context

AIQuotaPilot 当前架构：

```
package/contents/ui/main.qml              PlasmoidItem (Timer × 3 + Connections + CompactView + FullView)
package/contents/ui/CompactView.qml       Orb（compactRepresentation）：PieChart/Bar + providerSwitch 动画
package/contents/ui/FullView.qml          面板（fullRepresentation）：header(H4 + 4 个 ToolButton) + ListView(ProviderGroup) + statusLabel
package/contents/ui/ProviderGroup.qml     供应商卡片：标题 + LED + Plans Repeater
package/contents/ui/ProviderEditor.qml    配置：基本信息 + 限额项 + 脚本编辑器
package/contents/js/mockData.js           显示模型派生 + 颜色/排序辅助
package/contents/js/providerCatalog.js    内置供应商定义（minimax / codex / …）
package/contents/config/main.xml          KConfig XT 配置存储
package/contents/config/config.qml        ConfigModel 入口（常规 / 供应商）
package/contents/config/ProvidersConfig.qml  供应商列表编辑
package/contents/config/GeneralConfig.qml    常规面板
docs/requirements.md                      需求基线
docs/usage-script-spec.md                 自定义脚本规范
```

约束：
- QML-only，无 C++ 编译
- Plasmoid configuration 通过 `Plasmoid.configuration.<key>` 读写，`providers` 是 JSON 字符串
- 旧配置可能缺 `enabled` / `logoPath` / `sortMode` 字段，必须兼容
- `docs/usage-script-spec.md` 已规定 `resetAt` 字段，无需改动

## Goals / Non-Goals

**Goals**
- 5 项功能一次性交付并通过 `plasmawindowed` 视觉验收
- 排序逻辑放在 `mockData.js` 纯函数中，便于在脑外/脚本测试
- 状态字段（`enabled` / `logoPath` / `sortMode`）在 `normalizeDefinitions` 与新读取层兜底
- 不引入 C++ 改动、不引入新依赖

**Non-Goals**
- 不做"按曲线分组"
- 不做云端 Logo 同步
- 不做集成测试（Plasma 6 单元测试框架繁重，本次只做 `mockData.js` 单测 + 视觉验收）
- 不动 `usage-script-spec.md` 已有的 `resetAt` 约定

## Decisions

### D1. 排序放在 `mockData.js` 纯函数
**为什么**：`mockData.js` 是 `.pragma library`，不能持有 QML 状态；纯函数 + QML 端 `property var sortedProviders: MockData.sortProviders(providers, ..., sortMode)` 派生即可。
**备选**：把排序放在 `main.qml` JS 内 → 拒绝，混入 QML 上下文，不利于单测。

### D2. `nextResetAt` 字段在 `displayPlan` 层补齐
**为什么**：脚本规范已允许 `resetVariable` 提取重置时间，但 mock 阶段没有；为支持按"nextReset"排序，必须在 `_displayPlan` 中补 `nextResetAt`（毫秒时间戳；不可用时为 -1 表示"未排"）。
**mock 阶段估算**：根据 `plan.id`（如 `five-hours` / `seven-days` / `thirty-days`）推断周期，加上当前时间得到下次重置 ≈ `now + 周期`。
**备选**：要求 user 显式配置 `resetText` 解析 → 拒绝，不友好。

### D3. Orb 切换动画用 `CrossFade` + 缩放，不用 `Opacity` 全黑过渡
**为什么**：`CrossFade` 一帧内两个 Item 同时半透明叠加，肉眼几乎无黑屏；保留 `ScaleAnimator` 0.92→1 缩放带来轻微"入场感"。
**备选 A**：继续用 `Opacity` 0→1 → 拒绝，仍有黑屏。
**备选 B**：直接换 `Charts.PieChart` fast-binding 复用同一组件 → 拒绝，无法避免 push 弹入感。

### D4. Logo 字段双轨：内置 SVG base64 + 本地路径
**为什么**：内置供应商（MiniMax / Codex / GLM / 云之声 / Gemini / Claude）logo 写死在 `providerCatalog.js`；自定义供应商经配置页输入 `logoPath`（绝对路径或 `file://` URL）。
**QML 端**：`Image` 组件先尝试 `source = logoPath`，失败回退到 `Text(firstChar)` 圆角矩形，`Image.status` 监听 `Error` 切回退。
**为什么不用 `QIcon::fromTheme`**：本项目刻意不引入额外的图标主题依赖。

### D5. 启用/禁用过滤在显示模型派生阶段完成
**为什么**：`buildDisplayProviders` 之后立即调用 `filterEnabled(definitions, snapshots)`，下游不直接见到禁用供应商；这样 `highlightTimer` / `nextProviderIndexWithUsage` / `eventHighlighted` 都自然不引用禁用供应商。
**备选**：在 `ProviderGroup` 层 `visible: enabled` 隐藏 → 拒绝，仍占 ListView 槽位且 Orb 轮训会卡住。

### D6. 官网跳转用 `Qt.openUrlExternally`
**为什么**：Plasma 6 QML 标准 API；等价于 `xdg-open`，沙箱/Applet 上下文可用。
**安全**：仅在 `website` 通过 `^https?://` 正则校验后才调用，避免 `javascript:` / `file://` 注入。

### D7. 手动刷新重置 Timer 用 `Timer.restart()`
**为什么**：QML `Timer` 有原生 `restart()` 方法，省去手动 stop+start。
**改动**：`main.qml` 把匿名 Timer 改为 `id: refreshTimer` 暴露；`refresh()` 内调用 `refreshTimer.restart()`。

## Risks / Trade-offs

- [R1] `Qt.openUrlExternally` 在 Plasma sandbox 不可用 → 已在 Plasma 6.x 默认放行；如失败保留 tooltip 提示。
- [R2] 用户输入 `logoPath` 路径无效导致 `Image` 频繁出错 → 接 `sourceChanged` 一次性回调，缓存 fallback 状态，避免反复触发。
- [R3] 排序模式 `custom` 依赖用户维护 `customOrder` 数组 → 当用户删了某个供应商，customOrder 自动剔除保留项；新建时可拖拽或下拉追加（v2 给出 placeholder）。
- [R4] 动画 `CrossFade` 在低帧率设备上仍可能不流畅 → 时长上限 200ms，可观察。
- [R5] 5 项功能交叉修改 `providers` JSON 字段 → `normalizeDefinitions` 兜底必须覆盖所有缺字段路径；首次 `git checkout` 旧配置的用户应能平滑升级。

## Migration Plan

1. 部署：在 master 分支直接提交，patch 版本号提升（如 `2026.7.x → 2026.8.0`）。
2. 旧配置兼容：`normalizeDefinitions` 末尾对每个 definition 强制 `enabled = true / logoPath = ""`/`catalogId` 字段识别后插入内置 logo。
3. 旧配置排序：`sortMode` 缺省为 `default`，保持现数组顺序。
4. 回滚：直接 `git revert`，无 schema 变更。

## Open Questions

- 内置固定供应商的官网 + Logo：master 阶段主人期望我联网查（agent-reach）或先做占位符？
- `customOrder` 字段是否本期实现？还是 v2 再做（本期 v1 仅"按 default"显示即可）？
- Orb 闪烁修复要不要顺带处理"颜色变化"那部分（目前只处理切换）？
- mockData.js 单一文件超 500 行，要不要顺手拆分（已超）？
