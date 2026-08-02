# 供应商价格显示与按量计费余额设计（Provider Pricing & PAYG Balance）

日期：2026-08-02
状态：已与用户多轮确认

## 背景与目标

AI Usage Watcher 当前只显示各厂商套餐的用量百分比，不体现费用。本变更新增两项能力：

1. **套餐/订阅价格**：各厂商配置页新增「套餐/订阅价格」表单项（非必填），保存后悬浮面板（fullRepresentation）厂商名称右侧显示价格，面板底部累计总价。
2. **按量计费厂商（DeepSeek）**：参考 cc-switch 的 DeepSeek 余额查询实现，将 DeepSeek 新增为内置固定厂商，C++ 客户端查询账户余额；配置页提供 API Key（进 KDE 钱包）、充值金额、充值时间（后两者非必填）。面板显示：充值金额、充值时间、总余额、已用金额；兼容紧凑视图水平柱状图与饼图两种显示方式。

## 用户已确认的决策

| 决策点 | 结论 |
|---|---|
| 「当天使用金额」数据来源 | API 响应有用量字段则直接用；否则由 **充值金额 − 余额** 推算 |
| DeepSeek 接入方式 | 新增内置固定厂商 + C++ 客户端 |
| 价格字段粒度 | 每个厂商一个价格（非必填） |
| 总价参与 | 按量计费厂商不参与套餐价格，但设置了充值金额时计入总价 |

## 鉴权分析（已核实）

DeepSeek 余额查询接口 `GET https://api.deepseek.com/user/balance`：

- **鉴权**：API Key，`Authorization: Bearer <api_key>`（官方文档 + cc-switch `src-tauri/src/services/balance.rs:74-80` 双重确认）
- 申请地址：https://platform.deepseek.com/api_keys
- 超时：15s（照抄 cc-switch）
- 响应（金额字段为字符串，需兼容数字/字符串两种类型）：
  ```json
  {
    "is_available": true,
    "balance_infos": [{
      "currency": "CNY",
      "total_balance": "110.00",
      "granted_balance": "10.00",
      "topped_up_balance": "100.00"
    }]
  }
  ```
- `total_balance` = `granted_balance` + `topped_up_balance`

**配置页因此需要 API Key 输入框**（与 MiniMax/CodexZH 同一套 KDE 钱包凭据 UI）。

## 数据模型扩展

provider definition（存于 KConfig `providers` JSON）新增 3 个非必填字段：

| 字段 | 类型 | 语义 | 适用 |
|---|---|---|---|
| `price` | number ≥ 0 | 套餐/订阅价格（¥） | 所有厂商 |
| `topUpAmount` | number ≥ 0 | 最近一次充值金额（¥） | 按量计费厂商 |
| `topUpDate` | string `YYYY-MM-DD` | 最近一次充值时间 | 按量计费厂商 |

### 归一化与校验

- `providerNormalize.js normalizeDefinitions`：
  - 固定厂商分支（`Object.assign` 预设处）扩展保留 `price` / `topUpAmount` / `topUpDate`（现状只保留 `enabled` / `logoPath`）
  - 自定义厂商分支同样透传归一化（非负数字或空、字符串日期）
- `providerConfig.js validateProvider`：
  - `price` / `topUpAmount`：空或非负有限数字，否则 `{ valid: false, message }`
  - `topUpDate`：空或 `YYYY-MM-DD` 可解析日期
  - 固定厂商严格相等校验（`JSON.stringify(candidate) !== JSON.stringify(preset)`）改为剔除上述三个用户可配置字段后再比较

## 功能 1：套餐/订阅价格

### 配置页（ProviderEditor.qml）

基本信息区新增「套餐/订阅价格」数字输入框（非必填，允许两位小数，单位 ¥）。固定/自定义厂商均可用。

### 面板显示（ProviderGroup.qml）

- 厂商名称右侧（sourceLabel 之前）新增价格标签，仅当 `price > 0` 时可见，格式 `¥12.5`
- displayProvider 的 buildDisplay 输出透传 `price`

### 底部总价（FullView.qml）

- 状态栏上方新增 `总价 ¥xxx` 标签，仅当合计 > 0 时可见
- 计算：`displayProvider.totalPrice(displayProviders)` = Σ(所有启用厂商 `price`) + Σ(按量计费厂商 `topUpAmount`)
- 显示在面板底部（statusLabel 附近），跟随面板 bar/pie 两种样式

## 功能 2：DeepSeek 按量计费厂商

### 内置预设（providerCatalog.js）

```js
{
  catalogId: "deepseek", label: "DeepSeek", id: "deepseek",
  providerName: "DeepSeek", vendor: "DeepSeek",
  website: "https://platform.deepseek.com/",
  sourceLabel: "余额",
  template: "%1 限额  %2/%3",        // 无「重置于」段（balance plan 无重置概念）
  plans: [{ id: "balance", planName: "账户余额", unit: "元" }]
}
```

- Logo：新增 `images/providers/deepseek.svg` 或先用首字母 fallback（providerRegistry 机制支持）
- 固定厂商严格校验需容忍 price/topUp 三字段（见上）

### C++ 客户端（src/deepseekclient.{h,cpp} + deepseekresponseparser.{h,cpp}）

查询语义逐行对齐 cc-switch `src-tauri/src/services/balance.rs` 的 `query_deepseek`：

1. `GET https://api.deepseek.com/user/balance`（固定端点，无多端点候选）
2. 请求头：`Authorization: Bearer <api_key>`、`Accept: application/json`，超时 15s
3. 错误映射（分类逻辑照抄 cc-switch，**文案中文化**保持项目一致性）：
   - 401/403 → `鉴权失败 (HTTP xxx)`（确定性，立即透出）
   - 其余非 2xx → `接口错误 (HTTP xxx)`
   - 网络/超时 → 瞬时错误通道（重试保留上次成功值）
   - 响应体非法 JSON → 确定性失败
   - `is_available=false` → `isValid=false` + `余额不足`
4. 解析：
   - `balance_infos[0]`：`total_balance` → `remaining`（兼容字符串/数字）、`currency` → `unit`（**CNY 映射为 "元"**，其余币种保留 API 原值，避免与 ¥ 前缀重复显示）
   - `granted_balance` / `topped_up_balance` 解析保留备用（当前不展示）

客户端骨架沿用项目现有 C++ 客户端框架（与 minimaxclient 同构）：
- Q_PROPERTY：`snapshot` / `loading` / `credentialConfigured` / `credentialStatus` / `credentialBusy` / `credentialError`
- Q_INVOKABLE：`refresh()` / `saveCredential(apiKey)` / `clearCredential()`
- API Key 存 KDE 钱包（复用 MiniMaxClient 的钱包读写模式，路径隔离）
- snapshot 结构（对齐 cc-switch UsageData 字段集）：

```json
{ "providerId": "deepseek", "statusLabel": "可用", "errorText": "",
  "plans": [{ "planId": "balance", "planName": "账户余额",
              "remaining": 87.5, "used": -1, "total": -1,
              "unit": "元", "resetText": "", "resetAt": 0,
              "extraText": "", "isValid": true, "invalidReason": "" }] }
```

快照 plan 新增可选字段 `remaining`（其余 snapshot 生产者不受影响）。

### 配置页（ProviderEditor.qml + ProvidersConfig.qml）

| 字段 | 类型 | 存储 | 必填 |
|---|---|---|---|
| API Key | 密码框 + 保存/清除按钮 | KDE 钱包 | 查询余额需要 |
| 充值金额 | 数字输入（非必填） | definition.topUpAmount | 否 |
| 充值时间 | 文本输入 `YYYY-MM-DD` + 格式校验 | definition.topUpDate | 否 |

- 价格/充值金额输入控件统一用 `TextField` + 校验器（允许两位小数；SpinBox 步进不适合金额输入），校验失败给出提示文案
- ProviderEditor：`isDeepSeek` 判断（`catalogId === "deepseek"`）复用现有凭据区块，新增「充值设置」小节
- ProvidersConfig.qml：`credentialBackendMethod` / `syncCredentialState` 增加 deepseek 分支（`saveDeepSeekApiKey` / `clearDeepSeekApiKey` / `refreshDeepSeekUsage`），按现有 minimax/codexzh 双副本模式新增第三份（一致性优先，泛化重构留作独立任务）

### 接入（main.qml + aiusagewatcherapplet.cpp）

- applet：注册 DeepSeekClient、暴露 `deepseekSnapshot` 等属性与 Q_INVOKABLE 方法
- main.qml：`applyDeepSeekSnapshot()` / `requestDeepSeekRefresh()` + `Connections onDeepSeekSnapshotChanged` + `refresh()` 内调用（完整参照 MiniMax 接入）

### 面板显示（displayProvider.js + PlanBar）

`_displayPlan` 对 deepseek 的 balance plan 特殊处理：

- **有充值金额**（`definition.topUpAmount > 0` 且 `remaining` 有效）：
  - `total = topUpAmount`，`used = clamp(topUpAmount − remaining, 0, topUpAmount)`
  - 进度条 = 消耗比例（绿色/黄色/红色按现有阈值）
  - 正文 `已用 ¥12.5 / 充值 ¥100`；`extraText = "剩余 ¥87.5 | 充值 08-01"`
  - `topUpDate` 为今天 → 标注「今日已用 ¥12.5」；否则「自充值以来已用 ¥12.5」（标注进 extraText）
  - **余额 ≥ 充值金额**（本次充值未消耗，余额含历史结余/赠送）→ `used = 0`、进度条 0%，文案标注「本次充值未消耗」而非「已用 ¥0」，避免误导
- **无充值金额**：`usedPercent = -1`（"—"），`extraText = "余额 ¥87.5"`（余额仍可见）
- **API 响应自带 used 字段**（未来）→ 优先用响应值，不推算

### 紧凑视图兼容（CompactView.qml / PanelPieView.qml）

纯数据驱动，**零改动**：
- 有充值金额 → `usedPercent` 有效，柱状图/饼图正常渲染消耗比例
- 无充值金额 → `usedPercent = -1`，bar 显示占位、pie 灰色，与现有「暂无数据」行为一致

## 错误处理与状态

- 未保存 API Key → 配置页提示「尚未保存 API Key」，面板 statusLabel 显示「未配置」
- 查询失败 → `errorText` 透出（中文文案，见客户端错误映射），面板显示错误
- 瞬时失败（网络/超时）→ 复用现有 keep-last-good 语义（C++ 端维持 snapshot 直到刷新成功）
- `is_available=false` → plan 显示「余额不足」（inValidReason）

## 数据流

```
配置(KConfig providers JSON)                C++ 后端
┌────────────────────────────┐   ┌──────────────────────────┐
│ definition {               │   │ DeepSeekClient           │
│   price, topUpAmount,      │   │  GET /user/balance       │
│   topUpDate, plans[] ... } │   │  → snapshot {plans[{     │
└─────────┬──────────────────┘   │     remaining,unit,...}]}│
          │ normalizeDefinitions └────────────┬─────────────┘
          ▼                                    ▼
   main.qml: providers = DisplayProvider.buildDisplay(definitions, snapshots)
          │
          ├─ FullView(bar) → ProviderGroup → PlanBar
          │     · 名称右侧 price → "¥xx"（功能1）
          │     · payg plan：进度条=已用/充值，extraText=剩余|充值时间（功能2）
          │     · 底部 totalPrice → "总价 ¥xxx"
          ├─ FullView(pie) → PanelPieView（数据驱动，零改动）
          └─ CompactView(bar/pie)：usedPercent 驱动（有充值→百分比；无→"—"）
```

## 向后兼容

- 已有 `providers` JSON 无新字段 → 归一化默认空/未定义，价格与充值区不显示，零迁移
- **DeepSeek 不会自动出现在已有用户配置中**——预期行为：用户在配置页「添加供应商」手动选择 DeepSeek 预设（与其它固定厂商一致）
- 快照 plan 新增 `remaining` 为可选字段，其余 snapshot 生产者（MiniMax/Codex/CodexZH/custom）不传即不受影响

## 测试

1. **C++ 单测**（参照现有 tst_minimaxresponseparser / tst_minimaxclient 模式）：
   - `tst_deepseekresponseparser.cpp`：正常响应（字符串金额）、多币种、401、非 2xx、非法 JSON、is_available=false
   - `tst_deepseekclient.cpp`：mock 网络下成功/失败/saveCredential/clearCredential 状态机
2. **JS 逻辑单测**：`tests/tst_displayProvider.qml` + `qmltestrunner`（复用现有 `tests/tst_fullView.qml` 模式，`-import package/contents/ui` 加载 JS 库）：payg 推算（有/无充值金额、余额>充值金额的 clamp 与「未消耗」标注）、`totalPrice` 累计（价格+充值金额混合）；并入 `run-plasma-smoke.sh` 执行
3. **QML 冒烟**：`tests/run-plasma-smoke.sh` 扩展（总价标签可见性、DeepSeek 面板元素、`tst_displayProvider` 执行）
4. **手动验证**：`plasmawindowed` 检查配置页三字段、面板价格/余额显示、两种紧凑样式、总价累计

## 影响文件清单

| 文件 | 改动 |
|---|---|
| `package/contents/js/providerCatalog.js` | +deepseek 预设 |
| `package/contents/js/providerNormalize.js` | 固定厂商保留 price/topUp 字段；自定义透传 |
| `package/contents/js/providerConfig.js` | 三字段校验；固定厂商比较剔除用户字段 |
| `package/contents/js/displayProvider.js` | buildDisplay 透传 price/topUp；payg 推算；totalPrice() |
| `package/contents/ui/config/ProviderEditor.qml` | +价格输入、+充值设置（deepseek 分支）、凭据区扩展 |
| `package/contents/ui/config/ProvidersConfig.qml` | credential 分支扩展 deepseek |
| `package/contents/ui/ProviderGroup.qml` | 名称右侧价格标签 |
| `package/contents/ui/FullView.qml` | 底部总价标签 |
| `src/deepseekclient.{h,cpp}` | 新增客户端 |
| `src/deepseekresponseparser.{h,cpp}` | 新增响应解析 |
| `src/aiusagewatcherapplet.{h,cpp}` | 注册 DeepSeekClient |
| `package/contents/ui/main.qml` | apply/request/Connections 接入 |
| `tests/cpp/tst_deepseek*.cpp` | 新增单测 |
| `tests/run-plasma-smoke.sh` | 冒烟扩展 |

## 非目标（YAGNI）

- 不做多币种换算（统一 ¥ 展示，DeepSeek 余额保留 API 原单位）
- 不做「充值历史」多笔记录（只有最近一笔）
- 不做日期选择器（文本输入 + 校验）
- 不为其它按量计费厂商（StepFun/SiliconFlow 等）扩展 —— cc-switch 的 BALANCE_PROVIDERS 检测机制已分析，如后续需要可平移到本项目
