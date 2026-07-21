# 自定义用量查询脚本规范

> 本文定义未来真实数据后端与当前 UI 的边界。当前迭代不执行或编辑脚本，但 mock 数据和展示模型必须使用同一语义。

## 1. 脚本结构

脚本返回对象字面量：

```js
({
  request: { url, method, headers, body? },
  extractor: function (response) { return { /* quota */ }; }
})
```

`extractor` 可返回单个 quota 或 quota 数组。预览、日志和 IPC 不得包含真实凭据。

### request

| 字段 | 类型 | 约束 |
|---|---|---|
| `url` | string | 必填，`http`/`https`；占位符位于字符串内 |
| `method` | string | 必填，仅 `GET` / `POST` |
| `headers` | object | 可选；键名不得含占位符 |
| `body` | string | 可选 |

支持 `{{baseUrl}}`、`{{apiKey}}`、`{{accessToken}}`、`{{userId}}`。预览仅使用哨兵值。

## 2. extractor quota

| 字段 | 类型 | 含义 |
|---|---|---|
| `planName` | string | 套餐名 |
| `used` | number | 已用额度 |
| `total` | number | 总额度 |
| `remaining` | number | 可选兼容输入；仅在 `used` 缺失且 total 有效时用于推导 `used = total - remaining` |
| `unit` | string | 单位；超过 8 字符或含空白时进入 `unitOverflow` |
| `isValid` | boolean | `false` 时不参与最紧张值计算 |
| `invalidMessage` | string | 失效原因 |
| `resetAt` | string | 重置时间点 |
| `extra` | string | 独立补充文本 |

字段使用 camelCase。百分比不由 extractor 提供，避免多个事实来源。

## 3. 归一化规则

每个 quota 转为 `DisplayPlan`：

| DisplayPlan 字段 | 规则 |
|---|---|
| `planName` | quota `planName`，缺失时使用 provider definition 中的名称 |
| `used` | 优先 quota `used`；否则由 `total - remaining` 推导 |
| `total` | quota `total` |
| `usedPercent` | `clamp(round(used / total * 100), 0, 100)`；无效时 `-1` |
| `usedPercentLabel` | 有效时 `${usedPercent}%`，否则 `—` |
| `usedText` | `used` 的本地化独立文本，不含 total |
| `totalText` | `total` 的本地化独立文本 |
| `unitText` | 长度不超过 8 且不含空白的 unit |
| `unitOverflow` | 过长或含空白的 unit |
| `resetText` | `resetAt` 的显示文本 |
| `extraText` | `extra` |
| `isInvalid` | `isValid === false` |
| `invalidReason` | `invalidMessage` |
| `barClass` | 按 `<85` 绿、`85..94` 黄、`>=95` 红派生；无效为灰 |

唯一百分比是 `usedPercent`，值越高越紧张。有效计划中最大值驱动 compact 和 provider LED。不得再用剩余百分比驱动颜色或最紧张值。

## 4. 模板边界

模板保存在 `ProviderDefinition.template`，不属于 quota 或 plan snapshot。占位符固定为：

- `%1` = `planName`
- `%2` = `usedText`
- `%3` = `totalText`
- `%4` = `resetText`

默认模板为 `%1 限额  %2/%3  重置于 %4`。`%2`、`%3` 必须使用独立字段，禁止给 `%2` 传入 `used / total` 组合字符串。

## 5. 示例

```js
({
  request: {
    url: "{{baseUrl}}/api/usage",
    method: "POST",
    headers: { "Authorization": "Bearer {{apiKey}}" },
    body: JSON.stringify({ userId: "{{userId}}" })
  },
  extractor: function (response) {
    return {
      planName: response.planName || "默认套餐",
      used: response.used,
      total: response.total,
      unit: "tokens",
      isValid: !response.error,
      invalidMessage: response.error || "",
      resetAt: response.resetTime,
      extra: response.note || ""
    };
  }
})
```

若返回 `used: 88, total: 100`，DisplayPlan 必须得到 `usedPercent: 88`、黄色 `barClass`、`usedText: "88"`、`totalText: "100"`。
