# HTTP+JS 用量脚本安全契约

当前版本会在应用设置或刷新后执行脚本声明的真实查询。C++ 先调用独立 worker 提取
`request`，完成 HTTP 请求后再把 JSON 响应交给同一 worker 的 `extractor`，最后按限额
项配置的变量生成运行时快照。

## 脚本入口

```javascript
({
  request: {
    url: "https://example.com/api/usage",
    method: "GET",
    headers: {}
  },
  extractor: function(response) {
    return {
      used: response.used,
      limit: response.limit,
      resetAt: response.resetAt
    };
  }
})
```

每个限额项用变量引用绑定 `extractor` 的返回键，例如已用量 `${used}`、限额总量
`${limit}`、到期时间 `${resetAt}`。变量必须为 `${name}` 格式；已用量和总量必须是
有限数字且总量大于 0。到期时间可留空，填写时由脚本返回适合直接展示的文本，例如
`07-27 00:00`。额外键忽略；缺失的用量变量保留“暂无数据”，缺失到期时间则不显示时间。

## 网络边界

- 非本机地址只允许 `https`；`localhost` / loopback 可用 `http` 做开发测试。
- 禁止重定向到不同源；请求超时 15 秒，响应体上限 1 MiB，请求体上限 64 KiB。
- `response` 是服务返回的 JSON 对象或数组，不暴露文件、进程、网络或 Qt 对象。
- 当前自定义脚本中的固定请求头会随脚本保存在 KConfig；在通用 KWallet 凭据入口完成前，
  不应把长期密钥直接写进脚本。

## 执行边界

- 独立低权限 worker 进程；不得在 plasmashell、Plasmoid QML 或原生 Applet 主进程执行。
- 每次执行有墙钟/CPU 超时和任务、输出大小上限。
- worker 崩溃只能影响当前 provider，UI 与其他 provider 必须继续运行。
- 定时刷新和手动刷新使用同一执行链。

## 编辑器边界

编辑器负责文本编辑、原生语法高亮、行号、插入提示、轻量格式化和契约校验，不直接拥有
网络与执行权限。“测试脚本”当前只检查 `request.url`、`extractor` 和限额变量是否齐全；
保存或应用设置后，可通过面板刷新按钮执行真实查询。若未来采用 Qt WebEngine，必须另行
评估包体、CSP 和本地资源隔离。

提示下拉使用 `request`、`extractor`、`response`、`used`、`limit`、`resetAt` 等原始
JavaScript 标识符，插入时不附加 `${}`。`${name}` 仅是限额项引用 `extractor` 返回键的
绑定语法。编辑区允许拖拽改变高度，并可在“不换行”和“自动换行”之间切换。
