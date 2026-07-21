# Design

## Context

基线：`docs/requirements.md`；技术设计：`docs/superpowers/specs/2026-07-21-kcm-and-refresh-button-design.md`；base ref：`ebbf3df2892413a10c64a2637d812dd2e961171e`。

## Decisions

1. 唯一指标是 0..100 的 `usedPercent`，越高越紧张；颜色仅由统一派生函数决定。
2. KConfig `providers` 只保存带稳定 ID 的 provider definitions；runtime snapshots 和 display model 仅在内存中。
3. `mockData.js` 暴露纯函数负责规范化、派生、刷新和 tightest；Timer 不写 KConfig。
4. `compactStyle` 只控制 CompactView；FullView delegate 只实例化 ProviderGroup，ProviderGroup 只实例化 PlanBar。
5. KCM 由 `contents/config/config.qml` 的 ConfigModel/ConfigCategory 注册；页面通过 `cfg_` 属性获得 Apply/Cancel。
6. provider 编辑使用一个 Dialog + 普通 Editor Item，按 ID 定位并在保存前完整校验。
7. `keepPanelOpen` 映射 `PlasmoidItem.hideOnWindowDeactivate`，不使用 always-on-top 命名或语义。
8. 只跟随系统主题，使用 Kirigami Theme/Units；不实现 colorScheme/groupBy。
9. `used`、`total`、`unit` 独立；provider template 的 `%1..%4` 分别为 planName/used/total/resetText。
10. release gate 同时需要逻辑测试、静态检查、安装一致性、零运行错误和实际套餐行可见证据。

## Risks

- 旧 providers JSON 混有快照字段：规范化时只提取定义；失败回退 seeds。
- 新 provider 当前无真实后端：显示灰色无数据，不伪造使用量。
- Plasma popup API 差异：沿用本机官方 Folder View 的 `hideOnWindowDeactivate`，测试外部点击行为。
- qmltestrunner 无法代替 shell 运行：保留安装后 plasmawindowed 日志和可见行验证。
