# Logo 与 CodexZH 响应修复规格

## 目标

- Codex 透明 Logo 保留白底；其它自带背景的 Logo 不再叠加底色。
- CodexZH 按真实响应显示本周已用 `30.020896 / 255 USD`，进度约 `11.77%`。

## 范围

- `package/contents/ui/ProviderGroup.qml`：Codex 白底，其它 Logo 容器透明。
- `src/codexzhresponseparser.cpp`：读取 `dailyBudget`、`weeklyBudget`、`weekUsed`、
  `remainQuota` 等真实字段；仅在预算字段缺失时用 quota points 除以 `500000` 回退。
- `tests/tst_providerGroup.qml`：验证两类 Logo 背景。
- `tests/cpp/tst_codexzhresponseparser.cpp`、`CMakeLists.txt`：用真实响应结构验证
  `used=30.020896`、`total=255` 及详情字段。

## 约束

- 不记录或输出 API Key。
- 不修改供应商配置格式和凭据存储。
- 缺少周预算与周配额时保持解析失败，不展示伪造额度。

## 验证

- CodexZH 解析器单元测试。
- ProviderGroup QML 测试。
- 构建、安装后重启 `plasma-plasmashell.service`，人工确认 Logo 和 `30.020896/255 USD`。
