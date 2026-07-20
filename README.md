# AI Usage Watcher

KDE Plasma 6 桌面小部件，实时监控各大模型厂家的模型套餐用量。

## 形式

- 桌面图标（compact）：始终在桌面右上的 SVG 圆球，显示已用 %
- 弹出框（full）：KDE 时钟风格的浮层，展示每个供应商的：套餐名、已用百分比、剩余/总量、重置时间点、扩展说明
- 弹出框右上角：配置 / 固定 两个按钮（仿 KDE 顶部时间小部件）
- 系统托盘：右键菜单（设置、退出），左键打开弹出框

## 计划

完整需求见 [docs/requirements.md](docs/requirements.md)。自定义脚本编写规范见 [docs/usage-script-spec.md](docs/usage-script-spec.md)。

## 构建

待 KDev / kdesrc-build / craft 接入；本仓库只装源码骨架与需求文档。

## 开发与验证

### 前置条件

- KDE Plasma 6 桌面环境
- `plasmawindowed` 命令（通常随 `plasma-desktop` 安装）

### 运行小部件

```bash
# 安装到本地 Plasma 小部件目录
cp -r package ~/.local/share/plasma/plasmoids/org.kde.plasma.aiUsageWatcher/

# 用 plasmawindowed 独立窗口运行（无需添加到面板）
plasmawindowed aiUsageWatcher
```

### 验证清单

1. 圆球显示最紧张供应商的已用百分比
2. 点击圆球展开完整视图，所有供应商卡片正常渲染
3. 等待 60 秒，Timer 触发数据刷新，数值波动
4. 颜色语义：≤5% 红色、≤15% 黄色、>15% 绿色
5. 错误状态：供应商有 errorText 时显示错误信息