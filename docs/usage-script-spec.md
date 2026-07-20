# 自定义用量查询脚本规范

> 本文档是 ai-desktop-pet 自定义用量查询脚本的**权威规格说明**。
> 任何与此文档冲突的旧计划、旧实现，以本文为准。

## 1. 脚本整体结构

脚本必须返回**对象字面量**，含两个键：

```js
({
  request: { url, method, headers, body? },
  extractor: function(response) { return { /* quota 字段 */ } }
})
```

| 键 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `request` | object | 是 | HTTP 请求配置；运行前会在 QuickJS 沙箱里 eval + 序列化检查 |
| `extractor` | function | 是 | 接收响应 body 解析后的对象，返回一个或多个 Quota 对象（数组形式） |

### 1.1 `request` 字段

| 子键 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `url` | string | 是 | 必须含 scheme (`http`/`https`)；占位符 `{{baseUrl}}` `{{apiKey}}` `{{accessToken}}` `{{userId}}` 替换时**必须位于单/双引号字符串内** |
| `method` | string | 是 | 仅 `GET` 或 `POST` |
| `headers` | object | 否 | 键值对；键名不能含占位符 |
| `body` | string | 否 | 字符串 body，可含 `{{apiKey}}` 等占位符 |

### 1.2 占位符

| 占位符 | 替换为 |
|---|---|
| `{{baseUrl}}` | provider 基础 URL |
| `{{apiKey}}` | API Key（系统密钥环取值） |
| `{{accessToken}}` | Access Token（系统密钥环取值） |
| `{{userId}}` | provider 配置的 user_id |

预览脚本不读真实密钥值，只用哨兵值替换——日志、测试快照、前端 IPC 永远**不会**包含真实凭据。

## 2. extractor 返回字段（extractor return shape）

extractor 可返回**单个对象**或**数组**。所有字段**均为可选**，但推荐按字段语义填：

| 字段 | 类型 | 含义 |
|---|---|---|
| `planName` | string | 套餐名（如 "5小时窗口" / "7天窗口" / "余额"） |
| `remaining` | number | 剩余额度 |
| `used` | number | 已用额度 |
| `total` | number | 总额度（与 `remaining` 一起决定百分比） |
| `unit` | string | 单位标识（如 "USD" / "tokens" / "requests" / "%"）；**超过 8 字符或含空白**会被视为脚本误塞的详情，改放到扩展字段 |
| `isValid` | boolean | 套餐是否有效；`false` 时弹出 `invalidMessage` |
| `invalidMessage` | string | 失效原因（仅在 `isValid=false` 时显示） |
| `resetAt` | string | 重置时间点（ISO 或 `YYYY-MM-DD HH:mm:ss` 形式字符串） |
| `extra` | string | 扩展字段，**自由补充要展示的文本**（如活动期、计费说明等） |

### 2.1 字段命名

extractor 在 JS 里写**驼峰**（`planName`/`isValid`/`invalidMessage`/`resetAt`/`extra`），Rust 端 `#[serde(rename_all = "camelCase")]` 自动转换；前后端统一使用驼峰。

## 3. 完整示例

```js
({
  request: {
    url: "{{baseUrl}}/api/usage",
    method: "POST",
    headers: {
      "Authorization": "Bearer {{apiKey}}",
      "User-Agent": "cc-switch/1.0"
    },
    body: JSON.stringify({ userId: "{{userId}}" })
  },
  extractor: function(response) {
    return {
      isValid: !response.error,
      planName: response.planName || "默认套餐",
      remaining: response.balance,
      used: response.used,
      total: response.total,
      unit: "USD",
      resetAt: response.resetTime,
      extra: response.note || ""
    };
  }
})
```

## 4. 渲染约定（前端 DisplayQuota）

每个 quota 经过 `toDisplayQuota()` 归一后输出到 `DisplayPlan`，UI 只读这个对象：

| DisplayPlan 字段 | 来源 | 用途 |
|---|---|---|
| `name` | `planName` | 套餐名 |
| `remainingText` | `remaining` 数字本地化 | "剩 38,224,484" |
| `remainingUnit` | `unit` 短字符串 | "USD" / "tokens" / "" |
| `unitOverflow` | `unit` 超 8 字符或含空白 | 单独行展示，避免撑爆 UI |
| `percent` | (remaining/total)*100 | 剩余 %（圆球副指标） |
| `usedPercent` | 100 - percent | 已用 %（圆球主指标） |
| `resetText` | `resetAt` | 重置时间点 |
| `extraText` | `extra` | 扩展展示文本 |
| `isInvalid` | `isValid === false` | 是否失效 |
| `invalidReason` | `invalidMessage` | 失效原因 |

UI 规则：
- `unit` 短且无空白 → 直接跟在数字后面
- `unit` 长或含空白 → 不拼到数字，移到 `unitOverflow` 单独行
- `extra` 始终单独成行（小字）
- `resetAt` 缺失的计划自动不出现在重置时间点列表（避免空行）
- `isValid=false` 的计划显示 `invalidMessage`，不参与圆球 % 计算