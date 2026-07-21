# Proposal: complete the minimal viable plasmoid

## Why

当前小部件能启动，但 full 套餐行因 QML 运行错误不可见，百分比颜色语义相反，KCM 为空壳，Timer 还把瞬时用量写入 KConfig。需要把运行、配置、展示和验证做成一个真实闭环。

## What Changes

- 统一 `usedPercent`：越高越紧张；`<85` 绿、`85..94` 黄、`>=95` 红，最大值驱动 compact。
- 分离持久化 ProviderDefinition 与仅内存 RuntimeSnapshot，刷新从 used/total 重建所有 display 字段。
- 修复 Plasma 6 API，compact 保留 pie/bar，full 永远复用 ProviderGroup/PlanBar。
- 以 `contents/config/config.qml` 的 ConfigModel/ConfigCategory 实现标准 KCM Apply/Cancel 和 provider CRUD。
- “保持面板打开”只控制 `hideOnWindowDeactivate`；不承诺窗口置顶。
- 补齐 tooltip、accessibility、刷新动画、状态栏、响应式布局、metadata 和安装路径。
- 建立 QML/JS 测试、静态/XML/安装一致性和 plasmawindowed 运行门槛。

## Capabilities

- `runtime-usage-model`: 安全规范化、派生和 mock 刷新。
- `plasma-ui`: compact/full popup 与主题化、可访问展示。
- `applet-configuration`: 标准 General/Providers KCM。
- `release-verification`: 静态、逻辑、安装和运行验证。

## Non-Goals

真实 HTTP、脚本编辑、KWallet、历史曲线、full 图表切换、按窗口分组、主题选择、系统全局主题修改、真正窗口置顶。

## Impact

会修改 QML/JS/KConfig/metadata/README 并新增测试；插件 ID 和安装目录保持 `aiUsageWatcher`。不增加 C++ 或外部运行依赖。
