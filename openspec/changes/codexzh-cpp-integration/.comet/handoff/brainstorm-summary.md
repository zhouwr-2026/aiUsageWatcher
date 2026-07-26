# Brainstorm Summary

- Change: codexzh-cpp-integration
- Date: 2026-07-26

## 确认的技术方案

### C++ 代码提取
- 从 stash@{1} 提取 4 文件到 src/：codexzhclient.cpp/h、codexzhresponseparser.cpp/h
- CMakeLists.txt 注册 SOURCES + 测试目标

### 配置表单 Logo 头像
- 所有内置供应商：顶端 64x64 圆形头像，固定显示内联 SVG，不可编辑
- 自定义供应商：点击头像弹出文件选择器，选择后保存到 logoPath
- 移除 ProviderEditor 现有 logoPath 输入框

### 认证输入差异
- API Key 类（MiniMax/CodexZH）：API Key 输入框 + 保存/清除按钮
- OAuth 类（Codex）："登录"按钮
- 自定义类：脚本输入框

### 任务分组
1. C++ 代码提取 + 构建注册
2. applet 桥接
3. QML snapshot 接入
4. 配置表单 Logo 头像改造
5. providerRegistry 周限额修正
6. 编译验证

## 关键取舍与风险

- [R1] CodexZH API 端点/响应格式基于子 agent 推测实现，需真实 API Key 验证
- [R2] KWallet 首次使用需用户授权弹窗
- [R3] 内置供应商 SVG logo 已在前次 change 中验证，无需修改

## 测试策略

- C++ 编译：cmake --build build
- 安装测试：kpackagetool6 --upgrade package
- 视觉验证：plasmawindowed 确认 Logo 显示、配置表单布局

## Spec Patch

无（已完整覆盖）