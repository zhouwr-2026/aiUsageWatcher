# 额度领航员 KCM 与刷新链路设计

本文记录当前实现采用的设计，需求优先级以 [`docs/requirements.md`](../../requirements.md)
为准。

## 1. 设计目标

- 界面遵循 KDE 原生设置页：单一标题/返回区、左标签右输入框、统一列宽、只使用外层
  OK / Apply / Cancel。
- 固定厂商只暴露稳定预设和凭据入口；自定义厂商提供限额变量与查询脚本。
- 配置定义持久化，网络快照只驻留内存；任何查询失败都不能展示旧数据或假数据。
- 自定义脚本和网络错误不能影响 plasmashell 的桌面、顶栏与任务栏。

## 2. 配置页面状态

`ProvidersConfig.qml` 的 `providersModel` 是 KCM 编辑期工作副本，`cfg_providers` 是交给
KConfig XT 的序列化边界。`ProviderEditor.qml` 只维护当前候选项。

保存顺序固定为：

1. 用户在编辑器修改候选项，字段更新不得重建当前输入 delegate。
2. 外层 Apply/OK 调用页面 `saveConfig()`。
3. 页面校验候选项；新增则 append，编辑则按原稳定 ID replace。
4. `syncWorkingValue()` 一次性更新 `cfg_providers`。
5. Plasma 将全部 `cfg_*` 写入 KConfig，运行实例收到配置变化后重建定义。

Cancel 不调用提交链。校验失败时不更新 `cfg_providers`，编辑页使用现有校验提示说明原因；
配置窗口最终是否关闭仍由 Plasma 外层管理。

## 3. 厂商与限额编辑

厂商选择位于首项。Codex、Claude Code、OpenCode Go、MiniMax、智谱 GLM、Kimi For
Coding、硅基流动、CodexZH 使用代码内预设；稳定 ID、名称、官网和套餐结构只读。

只有“自定义”显示：

- 供应商标识、名称和官网；
- 任意数量的限额名称、单位、已用量/总量/重置时间变量；
- `request/extractor` JavaScript 编辑器。

脚本提示插入原始标识符，不插入 `${}`；限额绑定框才使用 `${name}`。编辑器撑满正文，
支持行号、高亮、格式化、契约测试、自动换行和垂直拖拽调高。

## 4. 查询与刷新链路

```text
Timer / 面板刷新
  -> 原生后端按供应商查询
  -> Codex / MiniMax 原生适配器，或自定义脚本 worker
  -> RuntimeProviderSnapshot
  -> buildDisplayProviders()
  -> compact / Tooltip / popup
```

Codex 使用官方设备码登录并维护隔离账号；MiniMax API Key 保存到 KDE Wallet。自定义
供应商先由 worker 提取 `request`，C++ 完成受限网络请求，再由 worker 执行 `extractor`。
定时刷新与手动刷新必须共用这条链路。

## 5. 展示语义

- 百分比始终是 `used / total`，高亮部分表示已使用，浅灰底轨表示剩余。
- popup 展示全部供应商和全部有效限额；模型/套餐重复前缀在规范化阶段去除。
- compact Tooltip 只展示当前模型第一个限额项的文字摘要，不绘图、不枚举其余限额。
- 限额名称允许换行且不省略；无数据使用灰色占位，查询错误显示红色角标和原因。

## 6. 验证与部署

- 单元测试覆盖候选项校验、动态输入焦点、外层 `saveConfig()`、页面重建、脚本契约和三种
  UI 展示数据契约。
- 静态检查、C++ 测试和 `git diff --check` 通过后才安装。
- 安装后核对源码/安装副本哈希，重启 Plasma 后核对 service、PID、语言、KConfig 和日志；
  不能只凭安装命令输出宣称部署成功。
