# provider-ux-enhancements — Tasks

> 实施通过 subagent-driven-development 完成。13 个 commits 落地（70d756b..f6196ca）。
> Final whole-branch review: APPROVED, 0 Critical / 0 Important / 3 Minor（不影响功能）。

## 1. mockData.js 数据扩展

- [x] 1.1 `normalizeDefinitions` 末尾对每个 definition 兜底补 `enabled = true`、`logoPath = ""`、`customOrder = null` 字段
- [x] 1.2 `_displayPlan` 返回值补 `nextResetAt`（毫秒时间戳，mock 阶段根据 plan.id 周期估算；不可用时 -1）
- [x] 1.3 新增 `filterEnabled(definitions, snapshots)` 纯函数，返回已过滤的 `{definitions, snapshots}`
- [x] 1.4 新增 `sortProviders(displayProviders, mode, customOrder)` 纯函数，支持 `default` / `alphabetical` / `usedPercent` / `remainingPercent` / `nextReset` / `custom`
- [x] 1.5 新增 `firstCharFallback(name)` 辅助（中文取首字、英文取首字符，统一转大写）

## 2. providerCatalog.js 内置 Logo 字典

- [x] 2.1 为 `minimax` / `codex` / `glm` / `cloud-voice` / `gemini` / `claude` / `gpt` 注册 `logoSvg` 字段（base64 或内联 SVG 字符串）
- [x] 2.2 在 `providerOptions()` 输出中暴露 `logoSvg` 字段供配置页预览
- [x] 2.3 若内置供应商缺 `website`，补全为各官方主页（MiniMax / Codex / 等）

## 3. main.qml 排序派生 + 计时器重置

- [x] 3.1 `providers` 改为派生属性：`filterEnabled` + `sortProviders` 串起来
- [x] 3.2 匿名 Timer 改成 `id: refreshTimer`，`refresh()` 末尾调用 `refreshTimer.restart()`
- [x] 3.3 `sortMode` 读取 `Plasmoid.configuration.sortMode || "default"`，写入 `Plasmoid.configuration.sortMode` 由 FullView 按钮触发
- [x] 3.4 `customOrder` 读取 `Plasmoid.configuration.customOrder || ""` 字符串（JSON 数组），变更时回写

## 4. FullView.qml 排序按钮 + 状态栏

- [x] 4.1 headerActions 增加"排序"ToolButton（图标 `view-sort`），点击循环切换 `sortMode` 并写回配置
- [x] 4.2 ToolButton 动态 Accessible.name / ToolTip.text 反映当前模式
- [x] 4.3 `statusLabel` 末尾加 ` · 排序：<mode>` 一段

## 5. ProviderGroup.qml 官网跳转 + Logo 显示

- [x] 5.1 `providerNameLabel` 改为可点击 MouseArea：仅当 `website` 通过 `https?://` 正则校验时启用，提供 hover cursor 变化与 `Qt.openUrlExternally(website)` 信号
- [x] 5.2 标题 Row 左侧增加 24x24 Logo 槽：`Image` 加载 `logoPath` 或 `logoSvg`；失败时 fallback 到 `Text(firstChar)` 圆角矩形
- [x] 5.3 接收 `logoSource` / `logoChar` 两个新属性；`enabled` 字段供上层过滤后不再由 ProviderGroup 自行处理

## 6. CompactView.qml Orb 切换动画

- [x] 6.1 现有 `providerSwitch` SequentialAnimation 替换为 `CrossFade` + `ScaleAnimator`（CrossFade 不可用已降级为 `Behavior on providerIndex` + `PropertyAnimation` 缩放 + opacity 同步）
- [x] 6.2 缩放起始 0.94、时长 120ms、缓动 `OutCubic`
- [x] 6.3 移除整组件 `opacity` 1→0→1 切换；保留颜色由 `usageColor` 一次绑定

## 7. ProvidersConfig.qml 启用/禁用 UI

- [x] 7.1 每个供应商列表项增加"启用/禁用"Switch（绑定 `providerObj.enabled`），默认 true
- [x] 7.2 列表渲染前按 `enabled` 过滤（仅 UI 层过滤，运行时仍由 mockData.filterEnabled 兜底）
- [x] 7.3 列表项增加 Logo 缩略图（与 ProviderGroup 展示一致）

## 8. ProviderEditor.qml Logo 路径输入

- [x] 8.1 基本信息区增加 `logoPath` 输入框（Kirigami.TextField，含 placeholder `file://...`）
- [x] 8.2 输入变更调 `updateField("logoPath", value)`
- [x] 8.3 输入框左侧 24x24 预览：内置 catalog 直接显示 `logoSvg`，自定义根据 `logoPath` 加载

## 9. config/main.xml 新增字段

- [x] 9.1 group `ui` 增加 `sortMode`（string，默认 `default`）
- [x] 9.2 group `ui` 增加 `customOrder`（string，默认空）—— 序列化 JSON 数组

## 10. 视觉验收

- [x] 10.1 `kpackagetool6 --install` 退出 0（已 13 个 commit 全部验证）
- [x] 10.2 Orb 切换动画（CrossFade fallback 为 scale + opacity 同步）— final-branch-review 已确认（Minor: 总时长 240ms 略超 plan 上限 200ms）
- [x] 10.3 sortMode 循环按钮 + statusLabel 显示 — final-branch-review 已确认
- [x] 10.4 启用/禁用 + ProvidersConfig Switch — final-branch-review 已确认
- [x] 10.5 Logo 路径 + 自定义 logoPath — final-branch-review 已确认（Task 8 CRITICAL fix 后）

## 11. 回归

- [x] 11.1 旧配置（无 `enabled` / `logoPath` / `sortMode` 字段）首次加载行为不变 — `filterEnabled` + `||` 兜底已实现
- [x] 11.2 `kpackagetool6 --install` 安装到本地 Plasma 目录跑通 — 已验证
- [x] 11.3 `git diff` 对照 proposal/design，检查无多余改动 — final-branch-review 报告确认

---

## 提交记录

| Commit | Message |
|--------|---------|
| 70d756b | feat(provider-ux): add providerRegistry.js with SVG catalog defaults |
| 1d31f5c | feat(provider-ux): split data layer into registry/snapshot/display + mockData shim |
| 956de03 | feat(provider-ux): sort derivation + refresh Timer restart |
| eabdd4c | fix(task4): sortMode 变更后即时调用 refresh() 重排 providers |
| 312961b | feat(provider-ux): sortMode cycling button in FullView header |
| 1f6b731 | feat(provider-ux): provider website link + logo rendering |
| f8f5bfb | fix(task5): hide logo container when no logoSource and no logoChar |
| 76486bb | feat(provider-ux): CrossFade + scale animation for Orb switch |
| b0e4235 | fix(CompactView): add opacity animation to provider switch + remove redundant restart |
| 6b8de73 | feat(provider-ux): enable toggle + logo thumbnail in ProvidersConfig |
| 0b509fe | feat(provider-ux): logo path text field + thumbnail preview |
| 2216efa | fix(ProviderEditor): use ProviderRegistry.logoSvgFor instead of ProviderCatalog.definitionFor |
| f6196ca | feat(provider-ux): sortMode + customOrder schema fields |
