# 单一供应商配置设计

## 目标

所有额度领航员实例统一读取 `aiquotapilotrc` 中的一份供应商定义；正式 Plasma 面板的现有配置作为首次迁移来源。

## 范围

- 新增 `SharedProviderConfig`：校验、持久化并监听共享供应商 JSON。
- `AiUsageWatcherApplet` 向 QML 暴露共享值、首次迁移和保存方法。
- `main.qml` 优先读取共享值；共享值为空时才迁移当前实例值。
- `ProvidersConfig.qml` 从共享值初始化，并仅在 Apply/OK 的 `saveConfig()` 中保存。
- 增加持久化、无效输入和跨实例通知测试。

## 约束

- 不改供应商查询、凭据、快照和排序逻辑。
- 不记录供应商 JSON、脚本、Token 或 API Key。
- 共享 JSON 最大 4 MiB，必须为数组，且每项必须是包含 `plans` 数组的对象。
- 迁移前备份 Plasma 配置；先让正式 `plasmashell` 优雅退出并落盘，再启动新版本。

## 风险

- 首次启动顺序可能决定迁移来源；本次部署会先关闭 `plasmawindowed`，确保正式 `plasmashell` 首次迁移。
- 共享文件写入失败时保留旧值并返回失败，不用空值覆盖。

## 验证

- `tst_sharedproviderconfig`：保存后重开可读、无效 JSON 被拒绝、另一实例收到变更。
- QML 测试：设置页读取共享值并在 `saveConfig()` 写共享值。
- 完整构建、静态检查、安装副本比对。
- 重启 `plasmashell` 后正式面板与新 `plasmawindowed` 的供应商数量和 ID 一致。
