## 1. 提取子 agent C++ 代码并注册构建

- [x] 1.1 从 stash@{1} 提取 codexzhclient.cpp/h、codexzhresponseparser.cpp/h 到 src/ 目录
- [x] 1.2 修改 CMakeLists.txt 注册 4 个新源文件 + 添加 test 目标
- [x] 1.3 编译验证（`cmake --build build`），修复任何编译错误

## 2. applet 桥接 CodexZhClient

- [ ] 2.1 在 aiusagewatcherapplet.h 添加 Q_PROPERTY（codexzhSnapshot/codexzhLoading/credentialConfigured/credentialStatus/credentialBusy/credentialError）
- [ ] 2.2 在 aiusagewatcherapplet.cpp 添加 CodexZhClient 成员初始化、signal 连接、getter 方法
- [ ] 2.3 添加 Q_INVOKABLE 方法：refreshCodexZhUsage/saveCodexZhApiKey/clearCodexZhApiKey
- [ ] 2.4 编译验证（`cmake --build build`）

## 3. QML 侧接入 CodexZH snapshot

- [ ] 3.1 main.qml 添加 codexzhSnapshot 读取 + applyCodexZhSnapshot() 函数
- [ ] 3.2 main.qml 添加 requestCodexZhRefresh() + Connections 监听 codexzhSnapshotChanged
- [ ] 3.3 main.qml refresh() 中串联 CodexZH 的刷新和 snapshot 应用

## 4. 配置表单 Logo 头像改造

- [ ] 4.1 ProviderEditor.qml 顶端添加 64x64 圆形头像区域：内置供应商显示固定内联 SVG（readOnly），自定义供应商点击弹出文件选择对话框
- [ ] 4.2 移除 ProviderEditor.qml 现有的 `logoPath` TextField 输入框
- [ ] 4.3 ProvidersConfig.qml 列表项 Logo 缩略图对齐新布局

## 5. providerRegistry.js 修正为周限额

- [ ] 5.1 将 codexzh plans 从 `[{ id: "daily", ... }, { id: "monthly", ... }]` 改为 `[{ id: "weekly", planName: "周限额", unit: "%" }]`
- [ ] 5.2 验证 resetPeriodSec 与周限额是否匹配（7*24*3600 或 30*24*3600）

## 6. 编译验证与集成测试

- [ ] 6.1 `cmake --build build` 编译通过
- [ ] 6.2 `kpackagetool6 --upgrade package` 安装通过，无 QML 运行时错误
- [ ] 6.3 确认 CodexZH 供应商在配置页可添加、Logo 正确显示、面板显示"未配置"状态