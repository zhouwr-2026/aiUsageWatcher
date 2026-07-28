# Compact 图表原生绘制改造设计

## 目标

将 compact 模式的饼图和水平柱状图对齐 Plasma 系统监控的原生绘制方式，
改善小尺寸下的抗锯齿效果。

## 范围

- 仅修改 `package/contents/ui/CompactView.qml` 的图表绘制实现。
- 保留当前尺寸、环宽、百分比文字、颜色阈值、动画、点击行为与错误徽标。
- 不修改数据模型、配置格式和弹出面板图表。

## 实现

- 移除 compact 饼图使用的 `Shape`、`ShapePath` 和 `PathAngleArc`。
- 使用一个 `Charts.PieChart`，通过数组数据源提供“已使用”和“剩余”两个扇区。
- 已使用扇区沿用 `usageColor()`；剩余扇区沿用当前轨道颜色。
- 设置与现有圆环一致的厚度和圆头，数据变化继续使用现有 300ms 动画能力；
  若 KQuickCharts 不支持同等动画，则保留供应商切换动画，不新增自定义动画层。
- 水平柱状图改用 KDE 原生 `QQC2.ProgressBar` 结构，并像上游实现一样完全覆写
  `contentItem` 和 `background` 为圆角 `Rectangle`，不依赖 Breeze 的默认主题渲染。
- 水平柱状图继续保留现有百分比文字、颜色和 300ms 宽度动画。

## 验收

- compact 饼图在 Plasma 面板中边缘平滑，无明显锯齿。
- compact 水平柱状图边缘平滑，填充比例与当前行为一致。
- `0`、正常百分比、`100` 和无数据状态显示正确。
- 现有 `tst_compactView.qml`、静态检查和安装级 Plasma 冒烟测试通过。

## 风险

KQuickCharts 和 QQC2 都是项目及当前系统已有依赖，不新增第三方依赖。测试对象名保持不变，
避免扩大测试改动范围；ProgressBar 的内容和背景均由项目控制，不恢复主题相关的不可见问题。
