# 验证入口

## 自动静态门

```bash
bash tests/run-static-checks.sh
```

该脚本用固定 Qt 6 路径运行完整 `qmltestrunner` 测试集和全部生产 QML 的
`qmllint`，再检查 KConfig XML、metadata/AppStream 必填字段，以及禁用的
Plasma 5 theme API、外部 KCM 声明和硬编码十六进制主题色。

## 安装级冒烟门

必须在已登录的 Plasma 桌面会话中运行：

```bash
bash tests/run-plasma-smoke.sh
```

脚本使用 CMake 构建并安装 QML 包和 C++ Applet 插件，通过
`kpackagetool6 --show aiUsageWatcher` 解析实际安装目录，并用 `diff -qr`
比较源码包和安装副本。只排除 `.directory`、`*.qmlc`、`*.jsc` 运行缓存，
不会排除任何 QML、JavaScript、XML 或 metadata 源文件；同时验证
`~/.local/lib/qt6/plugins/plasma/applets/aiUsageWatcher.so` 存在。

随后脚本运行 `tst_fullView.qml`。该测试明确断言 3 个 `providerGroup` 和
6 个 `planBar`，再启动一个由脚本自己记录 PID 的 `plasmawindowed`
进程。观察 8 秒后只向该 PID 发送 TERM，保存退出状态并拒绝日志中的
`ReferenceError`、`TypeError`、`PlasmaCore.Units`、
`Error loading QML file` 和插件加载错误。脚本还要求日志出现原生后端加载
证据；不使用 `timeout`，返回 124 也不会算通过。

KDE 官方参考：
[Plasma Widget Testing](https://develop.kde.org/docs/plasma/widget/testing/)、
[Plasma Widget Setup](https://develop.kde.org/docs/plasma/widget/setup/)。

## 发布前人工证据

自动测试证明组件树数量，但不替代真实 popup 截图。发布前在 Plasma 面板
添加“额度领航员”，点击 compact 视图打开 popup，并保存一张同时清楚
显示以下内容的截图：

- 已配置的 provider 全部出现；未接入或未登录的厂商显示灰色占位而不是演示数值；
- Codex 登录后显示接口实际返回的 5 小时/周等窗口，MiniMax 配置 Key 后显示
  当前周期/每周窗口；
- header 的刷新、配置、保持打开、关闭按钮和底部最近刷新状态。
- compact 按配置间隔轮巡供应商，首个限额项文字 Tooltip 与图标当前值一致；右键菜单只有一个
  Plasma 标准“配置…”入口。
- 分别切换 popup 水平柱状图和环形饼图布局，确认模型/窗口数量一致。
- 启用 D-Bus 事件优先，发送 `ModelActivated` 后确认立即切换、金色高亮并按时恢复。

再点击配置按钮，人工检查 KCM：

1. `General` 和 `Providers` 两个分类都能打开；
2. 在 `General` 修改 compact 样式，点 Cancel 后不保存，点 Apply 后保留；
3. 在 `Providers` 编辑供应商工作副本，Cancel 不保存，Apply 后重新打开仍保留；
4. popup 与 KCM 均无明显截断、重叠或大面积空白。
5. 供应商编辑页的“厂商选择”位于第一项，固定厂商字段自动填充且只读；切换到自定义后，
   限额项与脚本编辑器出现，所有标签保持在输入框左侧且输入列对齐。
6. 脚本区显示连续行号和语法颜色；“格式化”不破坏模板，“测试脚本”明确提示没有执行
   网络请求或 JavaScript；编辑页不出现第二组完成/取消按钮。
7. 选择 Codex 后显示浏览器设备码登录入口；完成后账号邮箱进入列表，可继续添加其他账号，
   默认账号有标记，删除操作只移除所选 QuotaPilot 隔离账号；登录状态与额度查询状态分开显示。
8. 选择 MiniMax 后，凭据保存状态与额度查询状态分开显示；“刷新额度”能即时触发查询，
   失败时显示具体错误而不是继续展示旧额度。

若当前没有 DISPLAY/Wayland 或 D-Bus 桌面会话，smoke 脚本返回
`BLOCKED`，不得把这种环境阻塞写成通过。
