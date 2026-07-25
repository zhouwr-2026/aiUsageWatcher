# provider-ux-enhancements — Tasks

## 1. mockData.js 数据扩展

- [ ] 1.1 `normalizeDefinitions` 末尾对每个 definition 兜底补 `enabled = true`、`logoPath = ""`、`customOrder = null` 字段
- [ ] 1.2 `_displayPlan` 返回值补 `nextResetAt`（毫秒时间戳，mock 阶段根据 plan.id 周期估算；不可用时 -1）
- [ ] 1.3 新增 `filterEnabled(definitions, snapshots)` 纯函数，返回已过滤的 `{definitions, snapshots}`
- [ ] 1.4 新增 `sortProviders(displayProviders, mode, customOrder)` 纯函数，支持 `default` / `alphabetical` / `usedPercent` / `remainingPercent` / `nextReset` / `custom`
- [ ] 1.5 新增 `firstCharFallback(name)` 辅助（中文取首字、英文取首字符，统一转大写）

## 2. providerCatalog.js 内置 Logo 字典

- [ ] 2.1 为 `minimax` / `codex` / `glm` / `cloud-voice` / `gemini` / `claude` / `gpt` 注册 `logoSvg` 字段（base64 或内联 SVG 字符串）
- [ ] 2.2 在 `providerOptions()` 输出中暴露 `logoSvg` 字段供配置页预览
- [ ] 2.3 若内置供应商缺 `website`，补全为各官方主页（MiniMax / Codex / 等）

## 3. main.qml 排序派生 + 计时器重置

- [ ] 3.1 `providers` 改为派生属性：`filterEnabled` + `sortProviders` 串起来
- [ ] 3.2 匿名 Timer 改成 `id: refreshTimer`，`refresh()` 末尾调用 `refreshTimer.restart()`
- [ ] 3.3 `sortMode` 读取 `Plasmoid.configuration.sortMode || "default"`，写入 `Plasmoid.configuration.sortMode` 由 FullView 按钮触发
- [ ] 3.4 `customOrder` 读取 `Plasmoid.configuration.customOrder || ""` 字符串（JSON 数组），变更时回写

## 4. FullView.qml 排序按钮 + 状态栏

- [ ] 4.1 headerActions 增加"排序"ToolButton（图标 `view-sort`），点击循环切换 `sortMode` 并写回配置
- [ ] 4.2 ToolButton 动态 Accessible.name / ToolTip.text 反映当前模式
- [ ] 4.3 `statusLabel` 末尾加 ` · 排序：<mode>` 一段

## 5. ProviderGroup.qml 官网跳转 + Logo 显示

- [ ] 5.1 `providerNameLabel` 改为可点击 MouseArea：仅当 `website` 通过 `https?://` 正则校验时启用，提供 hover cursor 变化与 `Qt.openUrlExternally(website)` 信号
- [ ] 5.2 标题 Row 左侧增加 24x24 Logo 槽：`Image` 加载 `logoPath` 或 `logoSvg`；失败时 fallback 到 `Text(firstChar)` 圆角矩形
- [ ] 5.3 接收 `logoSource` / `logoChar` 两个新属性；`enabled` 字段供上层过滤后不再由 ProviderGroup 自行处理

## 6. CompactView.qml Orb 切换动画

- [ ] 6.1 现有 `providerSwitch` SequentialAnimation 替换为 `CrossFade` + `ScaleAnimator`
- [ ] 6.2 缩放起始 0.94、时长 120ms、缓动 `OutCubic`
- [ ] 6.3 移除整组件 `opacity` 1→0→1 切换；保留颜色由 `usageColor` 一次绑定

## 7. ProvidersConfig.qml 启用/禁用 UI

- [ ] 7.1 每个供应商列表项增加"启用/禁用"Switch（绑定 `providerObj.enabled`），默认 true
- [ ] 7.2 列表渲染前按 `enabled` 过滤（仅 UI 层过滤，运行时仍由 mockData.filterEnabled 兜底）
- [ ] 7.3 列表项增加 Logo 缩略图（与 ProviderGroup 展示一致）

## 8. ProviderEditor.qml Logo 路径输入

- [ ] 8.1 基本信息区增加 `logoPath` 输入框（Kirigami.TextField，含 placeholder `file://...`）
- [ ] 8.2 输入变更调 `updateField("logoPath", value)`
- [ ] 8.3 输入框左侧 24x24 预览：内置 catalog 直接显示 `logoSvg`，自定义根据 `logoPath` 加载

## 9. config/main.xml 新增字段

- [ ] 9.1 group `ui` 增加 `sortMode`（string，默认 `default`）
- [ ] 9.2 group `ui` 增加 `customOrder`（string，默认空）—— 序列化 JSON 数组

## 10. 视觉验收

- [ ] 10.1 `plasmawindowed aiUsageWatcher` 启动；手工验证 5 项验收场景
- [ ] 10.2 对比改前/改后动画：Orb 切换无明显黑屏闪烁
- [ ] 10.3 配置页改 sortMode 后面板立即重排
- [ ] 10.4 禁用某个供应商后，面板 + Orb 轮训均跳过
- [ ] 10.5 给自定义供应商配 logoPath 后面板显示对应图片

## 11. 回归

- [ ] 11.1 旧配置（无 `enabled` / `logoPath` / `sortMode` 字段）首次加载行为不变
- [ ] 11.2 `kpackagetool6 --install` 安装到本地 Plasma 目录跑通
- [ ] 11.3 `git diff` 对照 proposal/design，检查无多余改动
