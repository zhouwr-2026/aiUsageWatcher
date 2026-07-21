# AI Usage Watcher

KDE Plasma 6 桌面小部件，实时监控各大模型厂家的模型套餐用量。

## 形式

- 面板图标（compact）：按配置顺序每 5 秒轮巡供应商，以饼图或水平柱状图显示当前供应商最紧张套餐的已用百分比。
- 悬停提示：与当前轮巡项同步，显示供应商、套餐和已用百分比。
- 左键弹出框（full）：按供应商展示全部已返回套餐，包括 5 小时、周、月等额度。
- 右键菜单：使用 Plasma 标准配置入口，并提供手动刷新。
- 弹出框标题栏：刷新、配置和保持打开。

## 计划

完整需求见 [docs/requirements.md](docs/requirements.md)。自定义脚本编写规范见 [docs/usage-script-spec.md](docs/usage-script-spec.md)。

## 构建

项目包含 QML 界面和 C++ 原生查询后端，CMake 会同时安装两部分：

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

当前 MiniMax 开发版只从启动进程的 `MINIMAX_API_KEY` 环境变量读取凭据。
不要把 Key 写入源码、配置文件或日志；后续版本会增加配置表单和 KWallet 保存。

## 开发与验证

### 前置条件

- KDE Plasma 6 桌面环境
- `plasmawindowed` 命令（通常随 `plasma-desktop` 安装）
- Qt 6、Plasma 6、KF6 CoreAddons 和 CMake 开发包

### 运行小部件

```bash
# 构建并安装 QML 包与 C++ 插件
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build

# 用 plasmawindowed 独立窗口运行（无需添加到面板）
QT_PLUGIN_PATH="$HOME/.local/lib/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}" \
  plasmawindowed aiUsageWatcher
```

### 验证清单

1. compact 使用饼图或水平柱状图，并每 5 秒切换一个已配置供应商。
2. 悬停文案与当前 compact 的供应商、套餐和百分比一致。
3. 左键展开后能看到所有供应商的全部套餐；右键只出现一个“配置…”入口。
4. MiniMax 返回的剩余百分比会动态转换为已用百分比；未配置 Key 时显示明确灰色状态。
5. 颜色语义：已用 `<85%` 绿色、`85..94%` 黄色、`>=95%` 红色，无数据灰色。
6. 执行 `bash tests/run-static-checks.sh` 和 `bash tests/run-plasma-smoke.sh`。
