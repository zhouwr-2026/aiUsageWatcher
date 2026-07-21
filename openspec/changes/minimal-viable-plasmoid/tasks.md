# Tasks

- [x] 1. 建立 qmltestrunner 测试骨架；用 RED 测试锁定 ProviderDefinition/RuntimeSnapshot 规范化、阈值、最大值、MiniMax 88 和刷新派生同步，再实现纯函数至 GREEN。
- [x] 2. 修复 main/compact/PieChart/KConfig/metadata：Kirigami API、三层状态、Timer 不持久化、真实 tooltip、compact pie/bar、keepPanelOpen、统一 ID/Name/License；静态与单测 GREEN。
- [x] 3. 重构 full 为唯一 `ProviderGroup -> PlanBar` 渲染链，补状态栏、模板、响应式布局、按钮 tooltip/accessibility 与 300ms 旋转；组件测试断言 3 provider/5 PlanBar。
- [x] 4. 新增标准 `contents/config/config.qml` 和 General KCM，通过 cfg_ 实现四项设置的 Apply/Cancel；删除外部 KCM、colorScheme、groupBy、alwaysOnTop。
- [ ] 5. 实现 Providers KCM 工作副本、稳定 ID CRUD、单 Dialog 编辑器、校验、删除确认和模板预览；测试 Cancel 不持久化、Apply 序列化 definitions。
- [ ] 6. 建立静态/XML/metadata/安装一致性/运行日志检查脚本；用官方安装命令验证运行副本，日志零 ReferenceError/TypeError，套餐行实际可见。
- [ ] 7. 同步 README、AGENTS 架构说明与验收记录，执行完整测试、diff/range 审查并在每个子任务后形成独立提交。
