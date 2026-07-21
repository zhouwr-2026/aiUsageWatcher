# Task 2 报告

## 状态

完成 root/compact/PieChart/KConfig/metadata 接线，未修改任务外文件。

## RED

命令：

```bash
/usr/bin/env QT_QPA_PLATFORM=offscreen /usr/lib/qt6/bin/qmltestrunner -input tests/tst_compactView.qml -import package/contents/ui -v1
```

摘要：`3 failed`。旧 `CompactView` 运行时访问不存在的 `PlasmaCore.Units`，且缺少 `compactPercent`、`compactPie`、`compactBar` 行为定位节点。

## GREEN

命令：

```bash
/usr/bin/env QT_QPA_PLATFORM=offscreen /usr/lib/qt6/bin/qmltestrunner -input tests -import package/contents/ui
xmllint --noout package/contents/config/main.xml
/usr/lib/qt6/bin/qmllint package/contents/ui/main.qml package/contents/ui/CompactView.qml package/contents/ui/PieChart.qml
```

摘要：Qt 6 测试 `13 passed, 0 failed`；XML 与 QML 检查 exit 0。

## 改动与自审

- `main.qml`：配置 definitions、内存 snapshots、display providers 分层；`refresh()` 不写 KConfig；tooltip 显示 tightest provider/plan/percent；`hideOnWindowDeactivate: !keepPanelOpen`；向 FullView 传 opacity/keep-open 接口。
- `CompactView.qml`、`PieChart.qml`：使用 Task 1 `tightestUsage()`/`usageClass()` 和 `Kirigami.Theme/Units`，pie/bar 与空数据均有真实组件测试。
- `main.xml`：仅保留 providers、refreshIntervalSec、opacityPercent、compactStyle、keepPanelOpen。
- `metadata.json`：保持 `aiUsageWatcher` ID，补 Name/License，删除 `X-KDE-ConfigModule`。
- `tests/tst_compactView.qml`：挂载到可见 Quick Item，验证 88%、`—`、pie/bar 可见。

## KDE 官方 API 核对

- `https://api.kde.org/qml-org-kde-plasma-plasmoid-plasmoiditem.html`：`hideOnWindowDeactivate` 是可写 bool，语义为失焦时隐藏。
- `https://api.kde.org/qml-org-kde-kirigami-platform-units.html`：`gridUnit`、`smallSpacing` 是 Kirigami 语义尺寸。
- `https://api.kde.org/plasma-applet.html`：`configuration` 是 QML 可读写的 `KConfigPropertyMap`，写入会触发持久化，因此 Timer 不写配置。
- 本机 `/usr/share/plasma/plasmoids/org.kde.desktopcontainment/contents/ui/FolderViewLayer.qml`：pin 使用 `root.hideOnWindowDeactivate = !checked`；官方 plasmoid 示例使用 `Plasmoid.configuration` 与 `Kirigami.Units`。

## 顾虑

- `main.qml` 已消费 Task 3 将提供的 FullView `opacityPercent`、`keepPanelOpen`、`configureRequested`、`keepOpenChanged` 接口；Task 3 未落地前 qmllint 会给这些接口 warning，但 exit 0。
- `PieChart.data` 沿用既有公共属性，会触发 shadow warning；当前 FullView 仍消费该属性，待 Task 3 删除 full 内联 pie 后可独立改名。
