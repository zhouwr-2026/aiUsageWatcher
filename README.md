# 额度领航员（AIQuotaPilot）

额度领航员是 KDE Plasma 6 的大模型套餐额度监控小部件。项目代码名为 QuotaPilot，
并沿用内部插件 ID `AIQuotaPilot`，因此已有安装实例和 KConfig 不需要迁移；软件界面
统一显示中文名“额度领航员”。

## 当前能力

- compact 支持环形饼图或水平进度条，按配置顺序和可配置间隔轮询模型。
- D-Bus 活跃事件可立即切换模型并显示金色高亮，超时后恢复原模型。
- Tooltip 只用文字展示当前模型的第一个限额项、已用/总量和重置时间。
- popup 支持独立的水平柱状图和多模型环形饼图布局，含刷新、配置、保持打开和关闭操作。
- 模型可增删、排序；可从 8 个固定厂商预设或“自定义”中选择。
- 固定厂商自动填充标识、名称、官网和套餐结构；自定义模式可添加任意限额项，并用
  `${used}` / `${limit}` 变量绑定脚本返回值。
- 自定义供应商会执行 `request` 请求，将 JSON 响应交给独立 JavaScript worker 的
  `extractor`，再按每个限额项配置的变量生成真实用量快照。
- 配置表单固定为左标签、右输入框；脚本编辑器支持行号、原生高亮、提示插入、格式化、
  安全契约测试、自动换行和拖拽调高。
- Codex 登录后通过官方 ChatGPT 用量接口读取 5 小时/周等真实限额；访问令牌过期时自动
  刷新，登录凭据始终留在 C++ 后端和用户私有目录。
- MiniMax 通过 C++ 后端查询，API Key 保存在 KDE Wallet；也兼容 `MINIMAX_API_KEY`，
  并自动尝试中国区/国际区的官方 Coding Plan 端点。
- OpenCode Go 抓取官方控制台页面获取真实用量：`GET opencode.ai/workspace/{id}/go`
  并携带浏览器登录后的 auth Cookie（社区通用方案，参考
  github.com/ridho9/opencode-go-usage；官方 `/zen/go/v1/usage` 接口至今未上线），
  解析页面内 SolidJS 序列化的服务端用量（5 小时 / 每周 / 每月百分比与重置倒计时）。
  凭据（工作区 ID + Cookie）保存在 KDE Wallet，在供应商编辑页配置；Cookie 会
  周期性失效，失效时提示更新。
- 错误模型仍参与轮询，并在 compact 显示红色感叹号，不会继续展示失败前的旧额度。
- 配置通过 KConfig XT 持久化，保存后由 Plasma 即时应用。

HTTP+JS 已开放真实执行：网络请求由 C++ 负责，JavaScript 只在独立 worker 中解析
`request` 和响应；非本机地址强制 HTTPS，并限制同源重定向、请求时间与响应大小。
编辑器的“测试脚本”当前只做保存前契约校验，真实查询在应用设置或刷新后执行。本地 HTTP
事件回调仍未开放。固定厂商中目前 Codex、MiniMax 和 OpenCode Go 已接通真实用量，
其余预设未接入时显示暂无用量。
详细边界见 [需求基线](docs/requirements.md)、
[KCM 与刷新链路设计](docs/superpowers/specs/2026-07-21-kcm-and-refresh-button-design.md) 和
[脚本安全契约](docs/usage-script-spec.md)。

## 构建与安装

需要 Qt 6.6+、Plasma 6、KF6 CoreAddons、KF6 Config、KF6 Wallet、CMake。

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
cmake --install build
```

独立运行：

```bash
QT_PLUGIN_PATH="$HOME/.local/lib/qt6/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}" \
  plasmawindowed AIQuotaPilot
```

## D-Bus 活跃事件

启用“D-Bus 事件优先”后，发送模型 ID 或显示名称：

```bash
dbus-send --session --type=signal /QuotaPilot \
  org.kde.quotaPilot.ModelActivated string:minimax
```

信号路径为 `/QuotaPilot`，接口为 `org.kde.quotaPilot`，信号为
`ModelActivated(QString)`。未知模型会被忽略并写入本地日志，事件不会接触凭据。

## 验证

```bash
bash tests/run-static-checks.sh
cmake -S . -B build -DCMAKE_INSTALL_PREFIX="$HOME/.local"
cmake --build build
ctest --test-dir build --output-on-failure
```

安装级测试会写入用户本地 Plasma 目录，仅在准备验证真实桌面实例时执行：

```bash
bash tests/run-plasma-smoke.sh
```

## 结构

```text
package/contents/
├── config/                 KConfig XT 与 KCM 入口
├── js/                     厂商目录、脚本工具、定义规范化与配置校验
└── ui/
    ├── main.qml            数据流、刷新、轮询、事件接线
    ├── CompactView.qml     compact 饼图/进度条、错误与高亮
    ├── QuotaTooltip.qml    首个限额项文字 Tooltip
    ├── FullView.qml        popup 外壳与水平柱状图布局
    ├── PanelPieView.qml    popup 环形饼图布局
    └── config/             常规设置、模型管理、模型编辑
src/                        Codex、MiniMax、自定义查询、KWallet、D-Bus 与脚本 worker
tests/                      QML/JS、C++、静态与桌面冒烟测试
```

## 相关项目

- [cc-switch](/home/zhouwr/Project/CodeWorkspace/cc-switch-main) — 用户已有的另一桌面工具（Tauri 2），当需要参考其 UI/UX 模式、配置持久化思路或跨平台打包方式时可以查阅。
