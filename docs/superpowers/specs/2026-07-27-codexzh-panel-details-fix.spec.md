# CodexZH 面板细节修复

## 目标

修正供应商 Logo 背景表现，补齐 CodexZH 周重置时间与中文套餐名，并让被截断的详情支持悬停完整查看。

## 改动范围

- `package/contents/ui/ProviderGroup.qml`
  - 只有 Codex 使用白色圆形底；有自带底色的 Logo 不显示通用圆形底或外圈。
- `package/contents/ui/PlanBar.qml`
  - 右下角详情保持单行省略；鼠标悬停时用原生 ToolTip 显示完整详情。
- `src/codexzhresponseparser.{h,cpp}`、`src/codexzhclient.cpp`
  - 套餐名改为“周限额”。
  - 按自然周计算下一次周一 00:00，并传入现有 `resetText`。
  - ToolTip 详情按官网顺序输出 16 个中文字段，每个字段经现有分隔符独立成行。
  - 调用次数和 Token 使用中文数字分组的完整整数，不使用 `K/M` 缩写。
  - 优先采用接口提供的 `*Formatted` 金额，缺失时再格式化原始数值。
- `tests/cpp/tst_codexzhresponseparser.cpp`
  - 用固定当前时间验证套餐名、255 美元限额和下一次重置时间。

## 约束与风险

- 不修改供应商配置、凭据、接口请求或刷新逻辑。
- 接口没有独立的周重置字段；根据 CodexZH 规则，下一次重置固定为下周一 00:00。
- 不新增依赖或新的 UI 组件。

## 验收

- Codex 有白色圆底；MiniMax、CodexZH 保留图片自身背景，不再叠加通用圆底/外圈。
- CodexZH 显示“周限额”和非空的“重置于”时间。
- 右下角详情被截断时，悬停可看到完整文本。
- ToolTip 依次显示“今日调用”至“订阅到期”共 16 行中文信息。
- 定向测试、构建、安装通过，正式 plasmashell 重启后验证。
