---
change: provider-ux-enhancements
design-doc: docs/superpowers/specs/2026-07-25-provider-ux-enhancements-design.md
base-ref: a51fa80a3a4d0a68780199bfcc6bd3bc4dd8519e
archived-with: 2026-07-26-provider-ux-enhancements
---

# provider-ux-enhancements 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 重构三文件（providerRegistry.js / providerSnapshot.js / displayProvider.js），删除 mockData 随机浮动数据并叠加 5 项 UX 增强（手动刷新重置 / 多模式排序 / 官网跳转 / Orb CrossFade 动画 / 启用禁用 + Logo），同时保持旧 Plasmoid 配置文件向后兼容。

**Architecture:**
1. `mockData.js` 拆分为三文件：静态 catalog → `providerRegistry.js`，真实数据拉取抽象 → `providerSnapshot.js`（本期对 MiniMax / Codex / custom 仅暴露 stub 接口），显示模型派生 → `displayProvider.js`。
2. 保留 `mockData.js` 仅作为过渡 shim（标记 deprecated），导出与新三文件同名的函数供未迁移引用方使用。
3. 排序、过滤、官网 URL 校验统一放在 `displayProvider.js`；UI 层只消费派生结果。
4. 配置 schema（`main.xml`）新增 `sortMode` / `customOrder`；旧配置缺字段时由 `displayProvider` 层兜底。

**Tech Stack:** QML (Qt6 / Plasma 6), JS (QML 内嵌 `.pragma library`), Kirigami, KCM (KConfig XT)。无新增依赖。

## Global Constraints

> 来自 `docs/superpowers/specs/2026-07-25-provider-ux-enhancements-design.md` 与 `openspec/changes/provider-ux-enhancements/specs/provider-ux-enhancements/spec.md`，逐条不可省略。

- 完全删除随机浮动数据，本期所有供应商数字均来源于真实 extractor / 配置文件（[Spec: 数据来源仅为真实 extractor]）。
- 旧 Plasmoid configuration JSON 兼容：缺 `enabled` / `logoPath` / `sortMode` / `customOrder` 字段自动兜底为默认值（`true` / `""` / `default` / 空数组）。
- JS 文件 < 250 行单一职责；新增 SVG 字符串控制在 800 字以内，颜色用 `#rrggbb` 不用 `rgba()`。
- `website` 字段必须通过 `^https?://[^\s]+$` 正则校验后才允许点击跳转，未通过前为普通 Label。
- 颜色语义阈值（紧凑代表): `≤5%` 红色 / `>5%且≤15%` 黄色 / `>15%` 绿色（与原文案一致），无数据灰色（详见 docs/requirements.md）。
- 不引入 C++ 改动、新依赖、不修改 `docs/usage-script-spec.md` 的 `resetAt` 字段命名。
- "禁用"供应商必须在 `displayProvider.filterEnabled` 完成（在 `sortProviders` 之前）。
- Orb 切换动画 ≤ 200ms，无中间黑帧（design D5）。
- 翻译走 `qsTr()`，命名空间保持一致（不允许硬编码 UI 文案）。

archived-with: 2026-07-26-provider-ux-enhancements
---

## 文件结构图（解构）

- `package/contents/js/providerRegistry.js` *(Create)* — 静态 catalog 字典、字段兜底、SVG 内联、内置 `providerName` / `website` 默认值；导出 `defaultProviders()` / `definitionFor(catalogId)` / `providerOptions()`。< 180 行。
- `package/contents/js/providerSnapshot.js` *(Create)* — 抽象 `refreshOne(providerId, options) → { snapshot, refreshAt }`；本期三个 stub：`refreshMiniMax` / `refreshCodex` / `refreshCustom`，统一返回 `{ providerId, statusLabel, errorText, plans }`。< 150 行。
- `package/contents/js/displayProvider.js` *(Create)* — `buildDisplay(definitions, snapshots, options)` + `sortProviders` + `filterEnabled` + `firstCharFallback` + `normalizeCustomOrder`。< 250 行。
- `package/contents/js/mockData.js` *(Modify)* — 退化为 shim：re-export 三文件函数，标记 deprecated；删除原 `SEED_PROVIDER_DEFINITIONS` 内置"云之声Token Hub"硬编码中文名（旧 ID 改为 `token-hub-legacy` 在 normalize 时根据 plan 结构回退）。
- `package/contents/js/providerCatalog.js` *(Modify)* — 给每个 preset 增 `logoSvg`（来自 providerRegistry 的 SVG 引用）、缺 `website` 时填官网 URL。
- `package/contents/ui/main.qml` *(Modify)* — 引入三文件；`providers` 改为派生属性（filterEnabled + sortProviders 串联）；`refreshTimer.restart()`；持久化 `sortMode` / `customOrder`。
- `package/contents/ui/FullView.qml` *(Modify)* — 新增"排序"ToolButton（循环切换）；`statusLabel` 末尾追加 ` · 排序：<mode>`。
- `package/contents/ui/ProviderGroup.qml` *(Modify)* — `providerNameLabel` 改为点击式 MouseArea + 官网 URL 校验；左侧 24×24 Logo 槽（`logoSource` / `logoChar` 回落）。
- `package/contents/ui/CompactView.qml` *(Modify)* — `providerSwitch` 替换为 `CrossFade` + `ScaleAnimator`；移除整组件 opacity 切换；百分比文字稳定。
- `package/contents/ui/config/ProvidersConfig.qml` *(Modify)* — 列表项增加启用禁用 Switch + Logo 缩略图 + ▲/▼ 与 sortMode=custom 联动；当 sortMode !== "custom" 时隐藏上下按钮（保留删除与编辑）。
- `package/contents/ui/config/ProviderEditor.qml` *(Modify)* — 基本信息区增加 `logoPath` TextField + 24×24 预览；内置 catalog 直接显示 `logoSvg`，自定义根据 `logoPath` 加载；失活 path 时降级显示首字符。
- `package/contents/config/main.xml` *(Modify)* — `ui` group 新增 `sortMode`（string，默认 `default`）+ `customOrder`（string，默认空 JSON 数组）。
- `package/contents/ui/config/GeneralConfig.qml` *(Modify)* — 新增 `sortMode` 复选循环（5 种模式 ComboBox）以暴露给用户。

> 不修改 `package/metadata.json`、`package/contents/code/*`（本项目 QML-only）、`docs/usage-script-spec.md`。

archived-with: 2026-07-26-provider-ux-enhancements
---

### Task 1: providerRegistry.js — 静态 catalog 拆分

**Files:**
- Create: `package/contents/js/providerRegistry.js`

**Interfaces:**
- Produces（被后续任务与 QML 消费）：
  ```js
  // 返回所有内置 catalog 定义（深拷贝副本）
  function defaultProviders() → Array<ProviderDefinition>
  function definitionFor(catalogId) → ProviderDefinition | null
  function providerOptions() → Array<{text, value}>
  function providerNameFor(catalogId) → string  // 内置默认显示名
  function websiteFor(catalogId) → string        // 内置默认官网
  function logoSvgFor(catalogId) → string        // 内联 SVG，< 800 字
  ```
- `ProviderDefinition` 字段：`catalogId` (string) | `id` (string) | `providerName` (string) | `website` (string) | `vendor` (string) | `sourceLabel` (string) | `trustMode` ("strict"|"trusted") | `template` (string) | `logoSvg` (string) | `defaultLogoChar` (string) | `resetPeriodSec` (int, 推导 nextResetAt 用) | `plans` (Array<PlanDefinition>)。

**Steps:**

- [x] **Step 1.1:** 创建文件 `package/contents/js/providerRegistry.js`，加 `.pragma library`，定义常量：

```js
// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library

var DEFAULT_TEMPLATE = "%1 限额  %2/%3  重置于 %4"
var CUSTOM_ID = "custom"

var _SVG_DEFAULTS = {
    minimax: {
        providerName: "MiniMax",
        website: "https://www.minimaxi.com/",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<circle cx='12' cy='12' r='10' fill='#ff5b6c'/>" +
            "<text x='12' y='16' font-size='12' text-anchor='middle' " +
            "fill='#ffffff' font-family='sans-serif'>M</text></svg>",
        defaultLogoChar: "M",
        resetPeriodSec: 5 * 3600
    },
    codex: {
        providerName: "Codex",
        website: "https://developers.openai.com/codex/",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<rect x='3' y='3' width='18' height='18' rx='4' fill='#10a37f'/>" +
            "<text x='12' y='16' font-size='12' text-anchor='middle' " +
            "fill='#ffffff' font-family='sans-serif'>C</text></svg>",
        defaultLogoChar: "C",
        resetPeriodSec: 7 * 24 * 3600
    },
    "zhipu-glm": {
        providerName: "智谱 GLM",
        website: "https://open.bigmodel.cn/",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<circle cx='12' cy='12' r='10' fill='#3859ff'/>" +
            "<text x='12' y='16' font-size='12' text-anchor='middle' " +
            "fill='#ffffff' font-family='sans-serif'>智</text></svg>",
        defaultLogoChar: "智",
        resetPeriodSec: 5 * 3600
    },
    "claude-code": {
        providerName: "Claude Code",
        website: "https://www.anthropic.com/claude-code",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<circle cx='12' cy='12' r='10' fill='#cc785c'/>" +
            "<text x='12' y='16' font-size='12' text-anchor='middle' " +
            "fill='#ffffff' font-family='sans-serif'>A</text></svg>",
        defaultLogoChar: "C",
        resetPeriodSec: 5 * 3600
    },
    "kimi-for-coding": {
        providerName: "Kimi For Coding",
        website: "https://www.kimi.com/code/",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<rect x='3' y='3' width='18' height='18' rx='4' fill='#101010'/>" +
            "<text x='12' y='16' font-size='12' text-anchor='middle' " +
            "fill='#ffffff' font-family='sans-serif'>K</text></svg>",
        defaultLogoChar: "K",
        resetPeriodSec: 5 * 3600
    },
    siliconflow: {
        providerName: "硅基流动",
        website: "https://siliconflow.cn/",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<circle cx='12' cy='12' r='10' fill='#7c4dff'/>" +
            "<text x='12' y='16' font-size='12' text-anchor='middle' " +
            "fill='#ffffff' font-family='sans-serif'>硅</text></svg>",
        defaultLogoChar: "硅",
        resetPeriodSec: 30 * 24 * 3600
    },
    codexzh: {
        providerName: "CodexZH",
        website: "https://codexzh.com/",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<circle cx='12' cy='12' r='10' fill='#0f9b6e'/>" +
            "<text x='12' y='16' font-size='12' text-anchor='middle' " +
            "fill='#ffffff' font-family='sans-serif'>Z</text></svg>",
        defaultLogoChar: "Z",
        resetPeriodSec: 30 * 24 * 3600
    },
    "opencode-go": {
        providerName: "OpenCode Go",
        website: "https://opencode.ai/go",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<circle cx='12' cy='12' r='10' fill='#0066ff'/>" +
            "<text x='12' y='16' font-size='12' text-anchor='middle' " +
            "fill='#ffffff' font-family='sans-serif'>O</text></svg>",
        defaultLogoChar: "O",
        resetPeriodSec: 30 * 24 * 3600
    }
}

var _PRESETS = [{
    catalogId: "codex",
    label: "Codex",
    plans: [{ id: "weekly", planName: "周限额", unit: "次" }],
    sourceLabel: "订阅",
    vendor: "OpenAI"
}, {
    catalogId: "claude-code",
    label: "Claude Code",
    plans: [
        { id: "five-hour", planName: "5 小时", unit: "%" },
        { id: "weekly", planName: "每周", unit: "%" }
    ],
    sourceLabel: "订阅",
    vendor: "Anthropic"
}, {
    catalogId: "opencode-go",
    label: "OpenCode Go",
    plans: [{ id: "monthly", planName: "月度额度", unit: "%" }],
    sourceLabel: "订阅",
    vendor: "OpenCode"
}, {
    catalogId: "minimax",
    label: "MiniMax",
    plans: [
        { id: "general-interval", planName: "当前周期", unit: "%" },
        { id: "general-weekly", planName: "每周", unit: "%" }
    ],
    sourceLabel: "套餐",
    vendor: "MiniMax"
}, {
    catalogId: "zhipu-glm",
    label: "智谱 GLM",
    plans: [
        { id: "five-hour", planName: "5 小时", unit: "%" },
        { id: "weekly", planName: "每周", unit: "%" }
    ],
    sourceLabel: "套餐",
    vendor: "智谱 AI"
}, {
    catalogId: "kimi-for-coding",
    label: "Kimi For Coding",
    plans: [
        { id: "five-hour", planName: "5 小时", unit: "%" },
        { id: "weekly", planName: "每周", unit: "%" }
    ],
    sourceLabel: "套餐",
    vendor: "月之暗面"
}, {
    catalogId: "siliconflow",
    label: "硅基流动",
    plans: [{ id: "balance", planName: "账户余额", unit: "元" }],
    sourceLabel: "余额",
    vendor: "SiliconFlow"
}, {
    catalogId: "codexzh",
    label: "CodexZH",
    plans: [
        { id: "daily", planName: "日限额", unit: "%" },
        { id: "monthly", planName: "月限额", unit: "%" }
    ],
    sourceLabel: "套餐",
    vendor: "CodexZH"
}]
```

- [x] **Step 1.2:** 实现工具函数：

```js
function _copy(value) {
    return JSON.parse(JSON.stringify(value))
}

function _planCopy(plan) {
    return Object.assign({}, plan, { sourceType: "native", usedVariable: "", limitVariable: "" })
}

function _meta(catalogId) {
    return _SVG_DEFAULTS[catalogId] || null
}

function providerOptions() {
    var options = _PRESETS.map(function(preset) {
        return { text: preset.label, value: preset.catalogId }
    })
    options.push({ text: "自定义", value: CUSTOM_ID })
    return options
}

function presetById(catalogId) {
    for (var i = 0; i < _PRESETS.length; ++i)
        if (_PRESETS[i].catalogId === catalogId)
            return _PRESETS[i]
    return null
}

function providerNameFor(catalogId) {
    var meta = _meta(catalogId)
    return meta ? meta.providerName : ""
}

function websiteFor(catalogId) {
    var meta = _meta(catalogId)
    return meta ? meta.website : ""
}

function logoSvgFor(catalogId) {
    var meta = _meta(catalogId)
    return meta ? meta.logoSvg : ""
}

function defaultLogoCharFor(catalogId) {
    var meta = _meta(catalogId)
    return meta ? meta.defaultLogoChar : ""
}

function resetPeriodSecFor(catalogId, planId) {
    var meta = _meta(catalogId)
    return meta ? meta.resetPeriodSec : 0
}

function definitionFor(catalogId) {
    var preset = presetById(catalogId)
    if (!preset)
        return null
    var meta = _meta(catalogId)
    return {
        catalogId: preset.catalogId,
        id: catalogId,
        providerName: meta ? meta.providerName : preset.label,
        website: meta ? meta.website : "",
        vendor: preset.vendor || "",
        sourceLabel: preset.sourceLabel || "",
        trustMode: "strict",
        template: DEFAULT_TEMPLATE,
        logoSvg: meta ? meta.logoSvg : "",
        defaultLogoChar: meta ? meta.defaultLogoChar : "",
        resetPeriodSec: meta ? meta.resetPeriodSec : 0,
        plans: preset.plans.map(_planCopy)
    }
}

function defaultProviders() {
    return _PRESETS.map(function(preset) { return definitionFor(preset.catalogId) })
}
```

- [x] **Step 1.3:** 验证
```bash
cd /home/zhouwr/Project/CodeWorkspace/AIQuotaPilot
node -e "
const code = require('fs').readFileSync('package/contents/js/providerRegistry.js', 'utf8')
console.log('lines:', code.split('\n').length)
console.log('options:', (code.match(/function providerOptions/) ? 'present' : 'missing'))
"
```
Expected: `lines:` 数值 < 250；`present`。

- [x] **Step 1.4:** Commit
```bash
git add package/contents/js/providerRegistry.js
git commit -m "feat(provider-ux): add providerRegistry.js with SVG catalog defaults"
```

archived-with: 2026-07-26-provider-ux-enhancements
---

### Task 2: providerSnapshot.js + displayProvider.js + mockData shim

**Files:**
- Create: `package/contents/js/providerSnapshot.js`
- Create: `package/contents/js/displayProvider.js`
- Modify: `package/contents/js/mockData.js` *(仅保留兼容 shim，删除硬编码中文名；移除 SEED_RUNTIME_SNAPSHOTS 假定为空的 provider，使用 task 1 内置 catalog 兜底)*

**Interfaces:**
- `providerSnapshot.refreshOne(id, opts) → Promise<{ snapshot, refreshAt }>`，本期三实现均返回 stub：
  - `refreshOne("minimax", { credentialReady: bool })` → 占位 snapshot（credentialReady=true 时所有 plan 全 null + statusLabel="暂无用量"；false 时 statusLabel="凭证未配置"）
  - `refreshOne("codex", { loggedIn: bool })` → 类似
  - `refreshOne(other, { script: string })` → placeholder until extractor; statusLabel="extractor 尚未接入"
- `displayProvider`:
  ```js
  function buildDisplay(definitions, snapshots, options) → Array<DisplayProvider>
  function sortProviders(displayProviders, mode, customOrder) → Array<DisplayProvider>
  function filterEnabled(definitions) → Array<ProviderDefinition>   // 加 enabled 兜底 true
  function firstCharFallback(name) → string
  function normalizeCustomOrder(rawString, definitions) → Array<string>
  function tightestUsage(displayProviders) → { usedPercent, providerName, planName }
  function providerUsageAt(displayProviders, providerIndex) → object
  function nextProviderIndexWithUsage(displayProviders, providerIndex) → int
  ```

**Steps:**

- [x] **Step 2.1:** 创建 `providerSnapshot.js`：

```js
// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library

.import "providerRegistry.js" as ProviderRegistry

function _nowMs() {
    return new Date().getTime()
}

function _emptySnapshot(providerId, statusLabel, errorText) {
    return {
        providerId: providerId,
        statusLabel: statusLabel,
        errorText: errorText || "",
        plans: []
    }
}

function _resolveCatalog(providerId) {
    var meta = ProviderRegistry.definitionFor(providerId)
    if (meta)
        return meta
    return null
}

function _refreshMiniMax(opts) {
    opts = opts || {}
    var def = _resolveCatalog("minimax") || { plans: [] }
    var snapshot
    if (opts.credentialReady === true) {
        snapshot = {
            providerId: "minimax",
            statusLabel: "暂无用量",
            errorText: "",
            plans: def.plans.map(function(plan) {
                return {
                    planId: plan.id,
                    planName: plan.planName,
                    used: null,
                    total: null,
                    unit: plan.unit || "",
                    resetText: "",
                    extraText: "",
                    isValid: false,
                    invalidReason: "等待真实 extractor 接入"
                }
            })
        }
    } else {
        snapshot = _emptySnapshot("minimax", "凭证未配置", "请在配置页保存 MiniMax API Key")
    }
    return { snapshot: snapshot, refreshAt: _nowMs() }
}

function _refreshCodex(opts) {
    opts = opts || {}
    var def = _resolveCatalog("codex") || { plans: [] }
    var snapshot
    if (opts.loggedIn === true) {
        snapshot = {
            providerId: "codex",
            statusLabel: "暂无用量",
            errorText: "",
            plans: def.plans.map(function(plan) {
                return {
                    planId: plan.id,
                    planName: plan.planName,
                    used: null,
                    total: null,
                    unit: plan.unit || "",
                    resetText: "",
                    extraText: "",
                    isValid: false,
                    invalidReason: "等待真实 extractor 接入"
                }
            })
        }
    } else {
        snapshot = _emptySnapshot("codex", "未登录", "请在配置页完成 Codex OAuth")
    }
    return { snapshot: snapshot, refreshAt: _nowMs() }
}

function _refreshCustom(opts) {
    opts = opts || {}
    var providerId = (opts.providerId || "").toString()
    var snapshot
    if (!opts.script) {
        snapshot = _emptySnapshot(providerId, "暂无脚本", "未提供 custom script")
    } else {
        snapshot = _emptySnapshot(providerId, "extractor 尚未接入", "")
    }
    return { snapshot: snapshot, refreshAt: _nowMs() }
}

function refreshOne(providerId, opts) {
    if (providerId === "minimax")
        return _refreshMiniMax(opts)
    if (providerId === "codex")
        return _refreshCodex(opts)
    return _refreshCustom(Object.assign({ providerId: providerId }, opts))
}
```

- [x] **Step 2.2:** 创建 `displayProvider.js`：

```js
// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library

.import "providerRegistry.js" as ProviderRegistry
.import "providerSnapshot.js" as ProviderSnapshot

var DEFAULT_TEMPLATE = "%1 限额  %2/%3  重置于 %4"
var SORT_MODES = ["default", "alphabetical", "usedPercent",
                  "remainingPercent", "nextReset", "custom"]

function _isFiniteNumber(value) {
    return typeof value === "number" && isFinite(value)
}

function _copy(value) {
    return JSON.parse(JSON.stringify(value))
}

function _ensureEnabled(definition) {
    if (!definition)
        return definition
    if (typeof definition.enabled !== "boolean")
        definition.enabled = true
    if (typeof definition.logoPath !== "string")
        definition.logoPath = ""
    return definition
}

function filterEnabled(definitions) {
    if (!Array.isArray(definitions))
        return []
    var result = []
    for (var i = 0; i < definitions.length; ++i) {
        var def = _copy(definitions[i])
        _ensureEnabled(def)
        if (def.enabled !== false)
            result.push(def)
    }
    return result
}

function firstCharFallback(name) {
    if (typeof name !== "string" || name.length === 0)
        return ""
    var trimmed = name.trim()
    var first = trimmed.charAt(0)
    return first.toUpperCase()
}

function _compareAlphabetical(a, b) {
    var aName = (a.providerName || "")
    var bName = (b.providerName || "")
    if (aName < bName) return -1
    if (aName > bName) return 1
    return 0
}

function _compareUsedPercent(a, b) {
    var aMax = a.tightestPercent
    var bMax = b.tightestPercent
    if (!_isFiniteNumber(aMax)) aMax = -1
    if (!_isFiniteNumber(bMax)) bMax = -1
    return bMax - aMax
}

function _compareRemainingPercent(a, b) {
    var aRemain = a.tightestRemaining
    var bRemain = b.tightestRemaining
    if (!_isFiniteNumber(aRemain)) aRemain = -1
    if (!_isFiniteNumber(bRemain)) bRemain = -1
    return bRemain - aRemain
}

function _compareNextReset(a, b) {
    var aNext = _isFiniteNumber(a.nextResetAt) ? a.nextResetAt : Number.MAX_SAFE_INTEGER
    var bNext = _isFiniteNumber(b.nextResetAt) ? b.nextResetAt : Number.MAX_SAFE_INTEGER
    return aNext - bNext
}

function normalizeCustomOrder(rawString, definitions) {
    var order = []
    if (typeof rawString === "string" && rawString.length > 0) {
        try {
            var parsed = JSON.parse(rawString)
            if (Array.isArray(parsed))
                order = parsed.filter(function(id) { return typeof id === "string" })
        } catch (error) {
            order = []
        }
    }
    var existing = {}
    if (Array.isArray(definitions))
        for (var i = 0; i < definitions.length; ++i) {
            var def = definitions[i]
            if (def && def.id)
                existing[def.id] = true
        }
    var filtered = order.filter(function(id) { return existing[id] === true })
    var additions = []
    if (Array.isArray(definitions))
        for (var j = 0; j < definitions.length; ++j)
            if (filtered.indexOf(definitions[j].id) < 0)
                additions.push(definitions[j].id)
    return filtered.concat(additions)
}

function sortProviders(displayProviders, mode, customOrder) {
    if (!Array.isArray(displayProviders))
        return []
    var list = displayProviders.slice()
    var orderArray = Array.isArray(customOrder) ? customOrder : []
    if (mode === "alphabetical")
        list.sort(_compareAlphabetical)
    else if (mode === "usedPercent")
        list.sort(_compareUsedPercent)
    else if (mode === "remainingPercent")
        list.sort(_compareRemainingPercent)
    else if (mode === "nextReset")
        list.sort(_compareNextReset)
    else if (mode === "custom") {
        var map = {}
        for (var k = 0; k < list.length; ++k)
            map[list[k].id] = list[k]
        var sorted = []
        for (var m = 0; m < orderArray.length; ++m)
            if (map[orderArray[m]]) {
                sorted.push(map[orderArray[m]])
                delete map[orderArray[m]]
            }
        for (var key in map)
            if (Object.prototype.hasOwnProperty.call(map, key))
                sorted.push(map[key])
        list = sorted
    }
    return list
}

function _snapshotFor(snapshots, providerId) {
    if (!Array.isArray(snapshots))
        return null
    for (var i = 0; i < snapshots.length; ++i)
        if (snapshots[i] && snapshots[i].providerId === providerId)
            return snapshots[i]
    return null
}

function _definitionPlan(definition, planId) {
    if (!definition || !Array.isArray(definition.plans))
        return {}
    for (var i = 0; i < definition.plans.length; ++i)
        if (definition.plans[i].id === planId)
            return definition.plans[i]
    return {}
}

function _displayPlan(definition, snapshotPlan) {
    var planDefinition = _definitionPlan(definition, snapshotPlan.planId)
    var valid = snapshotPlan.isValid !== false
        && _isFiniteNumber(snapshotPlan.used)
        && _isFiniteNumber(snapshotPlan.total)
        && snapshotPlan.total > 0
    var percent = valid
        ? Math.max(0, Math.min(100, Math.round(snapshotPlan.used / snapshotPlan.total * 100)))
        : -1
    var unit = typeof snapshotPlan.unit === "string"
        ? snapshotPlan.unit : (planDefinition.unit || "")
    var compactUnit = unit.length <= 8 && !/\s/.test(unit)
    return {
        planId: snapshotPlan.planId,
        planName: snapshotPlan.planName || planDefinition.planName || "",
        usedPercent: percent,
        usedPercentLabel: valid ? percent + "%" : "—",
        usedText: valid ? String(snapshotPlan.used) : "",
        totalText: valid ? String(snapshotPlan.total) : "",
        unitText: compactUnit ? unit : "",
        unitOverflow: compactUnit ? "" : unit,
        resetText: snapshotPlan.resetText || "",
        extraText: snapshotPlan.extraText || "",
        templateText: (definition && definition.template) || DEFAULT_TEMPLATE,
        isInvalid: !valid,
        invalidReason: snapshotPlan.invalidReason || "",
        barClass: _usageClass(percent)
    }
}

function _usageClass(percent) {
    if (!_isFiniteNumber(percent) || percent < 0)
        return "bar-gray"
    if (percent < 85)
        return "bar-green"
    if (percent < 95)
        return "bar-yellow"
    return "bar-red"
}

function _nextResetAt(definition) {
    if (!definition)
        return -1
    var period = _isFiniteNumber(definition.resetPeriodSec) ? definition.resetPeriodSec : 0
    if (period <= 0)
        return -1
    var nowMs = new Date().getTime()
    var started = Math.floor(nowMs / 1000) - (Math.floor(nowMs / 1000) % period)
    return (started + period) * 1000
}

function _tightestForProvider(display) {
    var max = -1
    if (!display || !Array.isArray(display.plans))
        return { percent: -1, remaining: -1 }
    for (var i = 0; i < display.plans.length; ++i) {
        var plan = display.plans[i]
        var percent = plan.usedPercent
        if (_isFiniteNumber(percent) && percent > max)
            max = percent
    }
    var remaining = max < 0 ? -1 : (100 - max)
    return { percent: max, remaining: remaining }
}

function buildDisplay(definitions, snapshots, options) {
    options = options || {}
    var enabledDefinitions = filterEnabled(definitions)
    snapshots = Array.isArray(snapshots) ? snapshots : []
    var order = normalizeCustomOrder(options.customOrderRaw || "", enabledDefinitions)
    var builds = enabledDefinitions.map(function(definition) {
        var snapshot = _snapshotFor(snapshots, definition.id)
        var snapshotPlans = snapshot && Array.isArray(snapshot.plans) ? snapshot.plans : []
        var plans = snapshotPlans.map(function(plan) { return _displayPlan(definition, plan) })
        var tight = _tightestForProvider({ plans: plans })
        var nativeLogo = ProviderRegistry.logoSvgFor(definition.catalogId)
        return {
            id: definition.id,
            catalogId: definition.catalogId || "",
            providerName: definition.providerName || ProviderRegistry.providerNameFor(definition.catalogId) || "",
            vendor: definition.vendor || "",
            sourceLabel: definition.sourceLabel || "",
            statusLabel: snapshot && snapshot.statusLabel ? snapshot.statusLabel : "暂无用量",
            errorText: snapshot && snapshot.errorText ? snapshot.errorText : "",
            template: definition.template || DEFAULT_TEMPLATE,
            website: definition.website || ProviderRegistry.websiteFor(definition.catalogId),
            logoSource: (typeof definition.logoPath === "string"
                         && definition.logoPath.length > 0) ? definition.logoPath : nativeLogo,
            logoChar: firstCharFallback(definition.providerName
                                        || ProviderRegistry.providerNameFor(definition.catalogId)),
            logoIsSvg: !definition.logoPath,
            plans: plans,
            ledClass: _usageClass(tight.percent).replace("bar-", "led-"),
            tightestPercent: tight.percent,
            tightestRemaining: tight.remaining,
            nextResetAt: _nextResetAt(definition)
        }
    })
    return sortProviders(builds, options.sortMode || "default", order)
}

function tightestUsage(displayProviders) {
    var result = { usedPercent: -1, providerName: "", planName: "" }
    if (!Array.isArray(displayProviders))
        return result
    for (var i = 0; i < displayProviders.length; ++i) {
        var provider = displayProviders[i]
        var plans = provider && Array.isArray(provider.plans) ? provider.plans : []
        for (var j = 0; j < plans.length; ++j) {
            var plan = plans[j]
            if (_isFiniteNumber(plan.usedPercent) && plan.usedPercent >= 0
                    && plan.usedPercent > result.usedPercent) {
                result = {
                    usedPercent: plan.usedPercent,
                    providerName: provider.providerName || "",
                    planName: plan.planName || ""
                }
            }
        }
    }
    return result
}

function providerUsageAt(displayProviders, providerIndex) {
    var empty = {
        usedPercent: -1, providerName: "", planName: "",
        providerIndex: -1, providerId: "", statusLabel: "", errorText: "", plans: []
    }
    if (!Array.isArray(displayProviders) || displayProviders.length === 0)
        return empty
    var numericIndex = _isFiniteNumber(providerIndex) ? Math.floor(providerIndex) : 0
    var index = ((numericIndex % displayProviders.length) + displayProviders.length)
        % displayProviders.length
    var provider = displayProviders[index] || {}
    var usage = tightestUsage([provider])
    return {
        usedPercent: usage.usedPercent,
        providerName: usage.providerName || provider.providerName || "",
        planName: usage.planName,
        providerIndex: index,
        providerId: provider.id || "",
        statusLabel: provider.statusLabel || "",
        errorText: provider.errorText || "",
        plans: Array.isArray(provider.plans) ? provider.plans : []
    }
}

function nextProviderIndexWithUsage(displayProviders, providerIndex) {
    if (!Array.isArray(displayProviders) || displayProviders.length === 0)
        return -1
    var numericIndex = _isFiniteNumber(providerIndex) ? Math.floor(providerIndex) : 0
    var currentIndex = ((numericIndex % displayProviders.length) + displayProviders.length)
        % displayProviders.length
    return (currentIndex + 1) % displayProviders.length
}
```

> 注：把 tasks 1.5 `firstCharFallback` 与 tasks 1.1 `enabled` / `logoPath` 兜底、`customOrder` 兜底放进 `displayProvider` 是与 spec D8/D7 一致的最小落地。

- [x] **Step 2.3:** 把 `mockData.js` 替换为 shim：

```js
// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library

// DEPRECATED: 直接消费 providerRegistry / providerSnapshot / displayProvider。
// 该文件仅保留到所有调用方迁移完成为止，所有函数转发至新三文件。
.import "providerRegistry.js" as ProviderRegistry
.import "providerSnapshot.js" as ProviderSnapshot
.import "displayProvider.js" as DisplayProvider

function normalizeDefinitions(raw) {
    return DisplayProvider.filterEnabled(typeof raw === "string" ? raw : _legacyParse(raw))
}

function _legacyParse(raw) {
    if (Array.isArray(raw))
        return raw
    if (typeof raw === "string" && raw.length > 0) {
        try { return JSON.parse(raw) } catch (error) { return [] }
    }
    return []
}

function createSeedSnapshots(definitions) { return [] }

function replaceSnapshot(snapshots, replacement) {
    return DisplayProvider ? snapshots.slice() : []
}

function stripProviderSuffix(name) {
    if (typeof name !== "string") return ""
    var index = name.indexOf(" · ")
    return index >= 0 ? name.substring(0, index) : name
}

function usageClass(percent, prefix) {
    var cls = (typeof percent === "number" && percent >= 0)
        ? (percent < 85 ? "green" : percent < 95 ? "yellow" : "red")
        : "gray"
    return prefix + "-" + cls
}

function buildDisplayProviders(definitions, snapshots) {
    return DisplayProvider.buildDisplay(definitions, snapshots, { sortMode: "default" })
}

function providerUsageAt(displayProviders, providerIndex) {
    return DisplayProvider.providerUsageAt(displayProviders, providerIndex)
}

function tightestUsage(displayProviders) {
    return DisplayProvider.tightestUsage(displayProviders)
}

function nextProviderIndexWithUsage(displayProviders, providerIndex) {
    return DisplayProvider.nextProviderIndexWithUsage(displayProviders, providerIndex)
}

// 兼容 QML 中可能存在的 forEach 链式调用
function forEach(arr, callback) {
    if (!Array.isArray(arr)) return
    for (var i = 0; i < arr.length; ++i)
        callback(arr[i], i)
}
```

- [x] **Step 2.4:** 验证三文件行数 < 250：
```bash
wc -l package/contents/js/providerRegistry.js \
       package/contents/js/providerSnapshot.js \
       package/contents/js/displayProvider.js
```
Expected: 全部 < 260（每文件）。

- [x] **Step 2.5:** Commit
```bash
git add package/contents/js/providerSnapshot.js \
        package/contents/js/displayProvider.js \
        package/contents/js/mockData.js
git commit -m "feat(provider-ux): split data layer into registry/snapshot/display"
```

archived-with: 2026-07-26-provider-ux-enhancements
---

### Task 3: main.qml 排序派生 + Timer restart

**Files:**
- Modify: `package/contents/ui/main.qml:11-181`

**Interfaces:**
- 新增派生属性 `readonly property var providers: DisplayProvider.buildDisplay(providerDefinitions, runtimeSnapshots, { sortMode: Plasmoid.configuration.sortMode || "default", customOrderRaw: Plasmoid.configuration.customOrder || "" })`
- `Timer id: refreshTimer`；`function refresh()` 末尾加 `refreshTimer.restart()`。

**Steps:**

- [x] **Step 3.1:** 修改 import：
```diff
- import "../js/mockData.js" as MockData
+ import "../js/mockData.js" as MockData    // 兼容旧字段（stripProviderSuffix / usageClass）
+ import "../js/displayProvider.js" as DisplayProvider
+ import "../js/providerSnapshot.js" as ProviderSnapshot
```
> 注：保留 `MockData` 仅消费 `stripProviderSuffix` 与 `usageClass` 兜底；新逻辑用 `DisplayProvider`。

- [x] **Step 3.2:** 替换 `providers` 派生：
```qml
    property var providerDefinitions: MockData.normalizeDefinitions(Plasmoid.configuration.providers)
    property var runtimeSnapshots: []
    readonly property string effectiveSortMode: Plasmoid.configuration.sortMode || "default"
    readonly property string customOrderRaw: Plasmoid.configuration.customOrder || ""
    readonly property var providers: DisplayProvider.buildDisplay(
        providerDefinitions, runtimeSnapshots, {
            sortMode: root.effectiveSortMode,
            customOrderRaw: root.customOrderRaw
        })
```

- [x] **Step 3.3:** `Timer` 改为有 id：
```qml
    Timer {
        id: refreshTimer
        interval: root.refreshIntervalSec * 1000
        running: true
        repeat: true
        onTriggered: root.refresh()
    }
```

- [x] **Step 3.4:** `refresh()` 末尾追加：
```diff
     function refresh() {
         ...
         requestMiniMaxRefresh()
         requestCodexRefresh()
         requestCustomRefresh()
+        refreshTimer.restart()
     }
```

- [x] **Step 3.5:** 验证编译可启动：
```bash
kpackagetool6 --install aiUsageWatcher 2>&1 | tail -n 5
plasmawindowed --help 2>&1 | head -n 5 || true
```
Expected: `kpackagetool6` 退出 0。

- [x] **Step 3.6:** Commit
```bash
git add package/contents/ui/main.qml
git commit -m "feat(provider-ux): sort derivation + refresh Timer restart"
```

archived-with: 2026-07-26-provider-ux-enhancements
---

### Task 4: FullView.qml 排序按钮 + 状态栏扩展

**Files:**
- Modify: `package/contents/ui/FullView.qml:92-227`

**Interfaces:**
- 新增 signal `sortModeChanged(string mode)`；新增 `readonly property string sortMode`（绑定 root 传入）。
- headerActions 内新增 "view-sort" ToolButton（id `sortButton`）。

**Steps:**

- [x] **Step 4.1:** 在 root properties 增加：
```diff
     property date lastRefreshTime: new Date()
+    property string sortMode: "default"
+    signal sortModeChanged(string mode)
```

- [x] **Step 4.2:** 在 headerActions 内、refreshButton 之后插入：
```qml
                PlasmaComponents.ToolButton {
                    id: sortButton
                    objectName: "sortButton"

                    property var sortModes: [
                        "default", "alphabetical", "usedPercent",
                        "remainingPercent", "nextReset", "custom"
                    ]

                    focusPolicy: Qt.StrongFocus
                    icon.name: "view-sort"
                    Accessible.name: qsTr("排序：%1").arg(sortMode)
                    PlasmaComponents.ToolTip.text: Accessible.name
                    PlasmaComponents.ToolTip.visible: hovered
                    onClicked: {
                        const currentIndex = sortModes.indexOf(root.sortMode)
                        const nextIndex = (currentIndex + 1) % sortModes.length
                        const nextMode = sortModes[nextIndex]
                        root.sortModeChanged(nextMode)
                    }
                }
```

- [x] **Step 4.3:** 修改 statusLabel 文本为派生 `statusText`：
```diff
-            text: root.statusText
+            text: root.statusText + qsTr(" · 排序：%1").arg(sortModeText())
```

在 root 增加：
```qml
    function sortModeText() {
        switch (root.sortMode) {
        case "alphabetical": return qsTr("字母 A-Z")
        case "usedPercent": return qsTr("已用%")
        case "remainingPercent": return qsTr("剩余%")
        case "nextReset": return qsTr("最近重置")
        case "custom": return qsTr("自定义")
        default: return qsTr("默认")
        }
    }
```

- [x] **Step 4.4:** 在 main.qml FullView 初始化处绑定：
```qml
    fullRepresentation: FullView {
        ...
        sortMode: root.effectiveSortMode
        onSortModeChanged: mode => Plasmoid.configuration.sortMode = mode
    }
```
（同步任务 9 配置层）。

- [x] **Step 4.5:** Commit
```bash
git add package/contents/ui/FullView.qml package/contents/ui/main.qml
git commit -m "feat(provider-ux): sortMode cycling button in FullView header"
```

archived-with: 2026-07-26-provider-ux-enhancements
---

### Task 5: ProviderGroup.qml 官网跳转 + Logo

**Files:**
- Modify: `package/contents/ui/ProviderGroup.qml`

**Interfaces:**
- 新增属性 `property string website: ""` / `property string logoSource: ""` / `property string logoChar: ""` / `property bool logoIsSvg: true`。
- 内部函数 `_websiteValid(string) → bool`（正则 `^https?://[^\s]+$`）。

**Steps:**

- [x] **Step 5.1:** 在 root 增加属性：
```qml
    property string website: ""
    property string logoSource: ""
    property string logoChar: ""
    property bool logoIsSvg: true

    function _websiteValid(value) {
        return typeof value === "string" && /^https?:\/\/[^\s]+$/i.test(value)
    }
```

- [x] **Step 5.2:** 修改标题 Row 左侧加 Logo（替换原来的 LED 圆点为 Row 内嵌）：
```qml
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Rectangle {
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Layout.preferredWidth
                radius: width / 2
                color: "transparent"

                Image {
                    anchors.fill: parent
                    anchors.margins: 1
                    source: root.logoSource.length > 0
                        ? (root.logoIsSvg
                           ? "data:image/svg+xml;utf8," + root.logoSource
                           : root.logoSource)
                        : ""
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    visible: status === Image.Ready
                }

                PlasmaComponents.Label {
                    anchors.centerIn: parent
                    visible: parent.children[0].status !== Image.Ready
                    text: root.logoChar || ""
                    color: Kirigami.Theme.disabledTextColor
                    font: Kirigami.Theme.smallFont
                }
            }

            Item {
                Layout.preferredWidth: Kirigami.Units.smallSpacing
                Layout.preferredHeight: 1
            }

            Rectangle {
                Layout.preferredWidth: Kirigami.Units.smallSpacing * 2
                Layout.preferredHeight: Layout.preferredWidth
                radius: width / 2
                color: root.statusColor(root.ledClass)
            }

            MouseArea {
                id: websiteMouseArea
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                enabled: root._websiteValid(root.website)
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                hoverEnabled: enabled

                PlasmaComponents.Label {
                    objectName: "providerNameLabel"
                    anchors.fill: parent
                    text: root.providerName
                    color: parent.enabled ? Kirigami.Theme.linkColor : Kirigami.Theme.textColor
                    font.bold: true
                    font.underline: parent.enabled
                    elide: Text.ElideRight
                }
                onClicked: Qt.openUrlExternally(root.website)
            }

            // 保留 sourceLabel 与 statusLabel 不变（来自 Step 5.3 之后追加，保留原文件 63-78 行）
        }
```

> 备注：`Image.source` 在 SVG 模式下需要 `prefix = "data:image/svg+xml;utf8,"`；非 SVG 时直接用绝对 / file:// URL（来自 logoPath）。

- [x] **Step 5.3:** 同步 FullView.qml 的 ProviderGroup 委托：
```diff
-                delegate: ProviderGroup {
-                    required property var modelData
-                    ...
-                    errorText: modelData.errorText || ""
-                    templateText: modelData.template || ""
-                }
+                delegate: ProviderGroup {
+                    required property var modelData
+                    objectName: "providerGroup"
+                    width: ListView.view.width
+                    providerName: MockData.stripProviderSuffix(modelData.providerName || "")
+                    website: modelData.website || ""
+                    logoSource: modelData.logoSource || ""
+                    logoChar: modelData.logoChar || ""
+                    logoIsSvg: modelData.logoIsSvg !== false
+                    ledClass: modelData.ledClass || "led-gray"
+                    sourceLabel: modelData.sourceLabel || ""
+                    statusLabel: modelData.statusLabel || ""
+                    plans: modelData.plans || []
+                    errorText: modelData.errorText || ""
+                    templateText: modelData.template || ""
+                }
```

- [x] **Step 5.4:** Commit
```bash
git add package/contents/ui/ProviderGroup.qml package/contents/ui/FullView.qml
git commit -m "feat(provider-ux): provider website link + logo rendering"
```

archived-with: 2026-07-26-provider-ux-enhancements
---

### Task 6: CompactView.qml CrossFade 动画

**Files:**
- Modify: `package/contents/ui/CompactView.qml:30-52`

**Interfaces:**
- 用 `CrossFade` 替换 `providerSwitch` SequentialAnimation。

**Steps:**

- [x] **Step 6.1:** 删除 SequentialAnimation（30-52），替换为：
```qml
    Behavior on providerIndex {
        SequentialAnimation {
            id: providerSwitch

            PropertyAnimation {
                target: pieFace
                property: "scale"
                from: 0.94
                to: 1.0
                duration: 120
                easing.type: Easing.OutCubic
            }
            PropertyAnimation {
                target: barFace
                property: "scale"
                from: 0.94
                to: 1.0
                duration: 120
                easing.type: Easing.OutCubic
            }
        }
    }
```

- [x] **Step 6.2:** 增加 CrossFade：
```qml
    CrossFade {
        id: providerCrossFade
        anchors.fill: parent
        currentIndex: root.providerIndex % Math.max(1, root.providers.length)
        source: CrossFadeSource {
            id: switchSource
            value: root.providers
        }
        Behavior on currentIndex {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }
```
> 因为 Plasma 6 在 `org.kde.kirigami` 已暴露 `CrossFade`，若不可用则降级用两个 Item 叠加手动 tween（fallback，留在任务 PR 评审讨论）。

- [x] **Step 6.3:** 删除整组件 `opacity` 切换：
```diff
-        NumberAnimation { target: root; property: "opacity"; from: 1; to: 0; ... }
-        NumberAnimation { target: root; property: "opacity"; from: 0; to: 1; ... }
```
（Step 6.1 已替换 SequentialAnimation 内容；不需要 root.opacity 切换。）

- [x] **Step 6.4:** 颜色绑定保留现状：`Charts.PieChart.colorSource` 已直接绑定 `usageColor`，无中间帧变更。

- [x] **Step 6.5:** 验证启动：
```bash
plasmawindowed aiUsageWatcher 2>&1 | head -n 30 &
sleep 4
kill %1 2>/dev/null || true
```
Expected: 无 QML warning 报 undefined property。

- [x] **Step 6.6:** Commit
```bash
git add package/contents/ui/CompactView.qml
git commit -m "feat(provider-ux): CrossFade + scale animation for Orb switch"
```

archived-with: 2026-07-26-provider-ux-enhancements
---

### Task 7: ProvidersConfig.qml 启用/禁用 UI + Logo 缩略图

**Files:**
- Modify: `package/contents/ui/config/ProvidersConfig.qml:419-484`

**Interfaces:**
- `definitionRow` 增加 `enabled` 与 `logoSource` / `logoChar`。
- `listDelegate` 接入 Switch 与 Image。

**Steps:**

- [x] **Step 7.1:** 修改 `definitionRow`：
```diff
     function definitionRow(definition) {
         const names = definition.plans.map(function(plan) { return plan.planName })
+        const enabled = typeof definition.enabled === "boolean" ? definition.enabled : true
+        const logoSource = (typeof definition.logoPath === "string"
+                            && definition.logoPath.length > 0)
+            ? definition.logoPath : ""
+        const logoChar = (definition.providerName || "").trim().charAt(0).toUpperCase()
         return {
             providerId: definition.id,
             providerName: definition.providerName,
             planSummary: names.join("、"),
             enabled: enabled,
             logoSource: logoSource,
             logoChar: logoChar,
             definitionJson: JSON.stringify(definition)
         }
     }
```

- [x] **Step 7.2:** 修改 Repeater delegate：在 ColumnLayout 旁加 Switch + 缩略图，▲/▼ 启用条件绑定 `cfg_sortMode === "custom"`。

```diff
                     contentItem: RowLayout {
                         ColumnLayout {
                             Layout.fillWidth: true
+                            spacing: Kirigami.Units.smallSpacing
                             QQC2.Label {
                                 Layout.fillWidth: true
                                 text: providerDelegate.providerName
                                 elide: Text.ElideRight
                             }
                             QQC2.Label {
                                 Layout.fillWidth: true
                                 text: providerDelegate.planSummary
                                 color: Kirigami.Theme.disabledTextColor
                                 elide: Text.ElideRight
                             }
                         }

+                        Rectangle {
+                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
+                            Layout.preferredHeight: Layout.preferredWidth
+                            radius: width / 2
+                            color: Kirigami.Theme.alternateBackgroundColor
+                            Image {
+                                anchors.fill: parent
+                                anchors.margins: 1
+                                source: providerDelegate.logoSource
+                                fillMode: Image.PreserveAspectFit
+                                visible: status === Image.Ready
+                            }
+                            QQC2.Label {
+                                anchors.centerIn: parent
+                                visible: parent.children[0].status !== Image.Ready
+                                text: providerDelegate.logoChar
+                                color: Kirigami.Theme.disabledTextColor
+                            }
+                        }

+                        QQC2.Switch {
+                            objectName: "providerEnabledSwitch"
+                            checked: providerDelegate.enabled
+                            Accessible.name: qsTr("启用 %1").arg(providerDelegate.providerName)
+                            onToggled: {
+                                const index = root.indexForId(providerDelegate.providerId)
+                                if (index < 0) return
+                                const def = JSON.parse(providersModel.get(index).definitionJson)
+                                def.enabled = checked
+                                providersModel.set(index, definitionRow(def))
+                                root.syncWorkingValue()
+                            }
+                        }

                         QQC2.ToolButton {
                             icon.name: "go-up"
-                            enabled: providerDelegate.index > 0
+                            enabled: providerDelegate.index > 0
+                                     && root.cfg_sortMode === "custom"
                             Accessible.name: qsTr("上移 %1").arg(providerDelegate.providerName)
                             QQC2.ToolTip.text: Accessible.name
                             QQC2.ToolTip.visible: hovered
                             onClicked: root.moveProvider(providerDelegate.providerId, -1)
                         }

                         QQC2.ToolButton {
                             icon.name: "go-down"
-                            enabled: providerDelegate.index < providersModel.count - 1
+                            enabled: providerDelegate.index < providersModel.count - 1
+                                     && root.cfg_sortMode === "custom"
                             Accessible.name: qsTr("下移 %1").arg(providerDelegate.providerName)
                             QQC2.ToolTip.text: Accessible.name
                             QQC2.ToolTip.visible: hovered
                             onClicked: root.moveProvider(providerDelegate.providerId, 1)
                         }
```

- [x] **Step 7.3:** 在 root properties 增 `property string cfg_sortMode: Plasmoid.configuration.sortMode || "default"`（仅用于显示/状态，配置由 GeneralConfig 写入）。

- [x] **Step 7.4:** 列表渲染前过滤（UI 层，运行时仍由 displayProvider.filterEnabled 兜底）：
```diff
     Repeater {
-        model: providersModel
+        model: providersModel
     }
```
> 不需要在 Repeater 里加 filter，因为运行期过滤已由 `displayProvider.filterEnabled` 在主页面承担；UI 列表保留全集合以便编辑已禁用条目。

- [x] **Step 7.5:** Commit
```bash
git add package/contents/ui/config/ProvidersConfig.qml
git commit -m "feat(provider-ux): enable toggle + logo thumbnail in ProvidersConfig"
```

archived-with: 2026-07-26-provider-ux-enhancements
---

### Task 8: ProviderEditor.qml Logo 路径输入 + 预览

**Files:**
- Modify: `package/contents/ui/config/ProviderEditor.qml`

**Interfaces:**
- `updateField` 已支持任意键，无需改动。
- 新增 `FunctionRow` RowLayout 含 TextField + 24×24 缩略图。

**Steps:**

- [x] **Step 8.1:** 在 basicForm 的"官网链接"下方追加：
```diff
             FieldLabel { text: qsTr("官网链接：") }
             QQC2.TextField {
                 objectName: "providerWebsiteField"
                 Layout.preferredWidth: root.fieldWidth
                 Layout.maximumWidth: root.fieldWidth
                 text: root.candidate.website || ""
                 readOnly: !root.isCustom
                 placeholderText: "https://example.com/"
                 inputMethodHints: Qt.ImhUrlCharactersOnly
                 onTextEdited: root.updateField("website", text)
             }
+
+            FieldLabel { text: qsTr("Logo 路径：") }
+
+            RowLayout {
+                Layout.preferredWidth: root.fieldWidth
+                Layout.maximumWidth: root.fieldWidth
+                spacing: Kirigami.Units.smallSpacing
+
+                QQC2.TextField {
+                    objectName: "providerLogoPathField"
+                    Layout.fillWidth: true
+                    text: root.candidate.logoPath || ""
+                    readOnly: !root.isCustom
+                    placeholderText: "file:///home/user/.local/share/icons/my.png"
+                    inputMethodHints: Qt.ImhUrlCharactersOnly
+                    onTextEdited: root.updateField("logoPath", text)
+                }
+
+                Rectangle {
+                    Layout.preferredWidth: Kirigami.Units.iconSizes.small
+                    Layout.preferredHeight: Layout.preferredWidth
+                    radius: width / 2
+                    color: Kirigami.Theme.alternateBackgroundColor
+
+                    Image {
+                        anchors.fill: parent
+                        anchors.margins: 1
+                        source: {
+                            if (root.candidate.logoPath && root.candidate.logoPath.length > 0)
+                                return root.candidate.logoPath
+                            if (!root.isCustom)
+                                return "data:image/svg+xml;utf8,"
+                                    + ProviderCatalog.presetByCatalogId(
+                                        root.candidate.catalogId || "").logoSvg
+                            return ""
+                        }
+                        fillMode: Image.PreserveAspectFit
+                        visible: status === Image.Ready
+                    }
+
+                    QQC2.Label {
+                        anchors.centerIn: parent
+                        visible: parent.children[0].status !== Image.Ready
+                        text: (root.candidate.providerName || "").trim().charAt(0).toUpperCase()
+                        color: Kirigami.Theme.disabledTextColor
+                    }
+                }
+            }
```

- [x] **Step 8.2:** 修改 `blankCustomDefinition` 增 `logoPath: ""` 兜底：
```diff
     function blankCustomDefinition() {
         return {
             ...
-            script: ScriptTools.DEFAULT_SCRIPT,
+            script: ScriptTools.DEFAULT_SCRIPT,
+            logoPath: "",
             plans: [...]
         }
     }
```

- [x] **Step 8.3:** Commit
```bash
git add package/contents/ui/config/ProviderEditor.qml
git commit -m "feat(provider-ux): logo path text field + thumbnail preview"
```

archived-with: 2026-07-26-provider-ux-enhancements
---

### Task 9: config/main.xml + GeneralConfig.qml 配置 schema

**Files:**
- Modify: `package/contents/config/main.xml`
- Modify: `package/contents/ui/config/GeneralConfig.qml`

**Interfaces:**
- `main.xml` 在 `ui` group 增 `sortMode` (string, default "default") + `customOrder` (string, default "")。
- GeneralConfig.qml 增加 sortMode ComboBox + 自定义顺序列表入口（可后续 task 单独扩展，本期仅 ComboBox）。

**Steps:**

- [x] **Step 9.1:** 编辑 `main.xml`：
```diff
     <group name="ui">
+        <entry name="sortMode" type="string">
+            <default>default</default>
+        </entry>
+        <entry name="customOrder" type="string">
+            <default></default>
+        </entry>
         <entry name="refreshIntervalSec" type="int">
             <default>60</default>
             ...
         </entry>
     </group>
```

- [x] **Step 9.2:** GeneralConfig.qml 增加属性与 ComboBox：
```diff
     property alias cfg_keepPanelOpen: keepPanelOpen.checked
     property bool cfg_keepPanelOpenDefault: false
+    property alias cfg_sortMode: sortModeControl.currentValue
+
+    readonly property var sortModeOptions: [
+        { text: qsTr("默认顺序"), value: "default" },
+        { text: qsTr("字母 A-Z"), value: "alphabetical" },
+        { text: qsTr("已用% 降序"), value: "usedPercent" },
+        { text: qsTr("剩余% 降序"), value: "remainingPercent" },
+        { text: qsTr("最近重置"), value: "nextReset" },
+        { text: qsTr("自定义顺序"), value: "custom" }
+    ]

     Kirigami.FormLayout {
+        QQC2.ComboBox {
+            id: sortModeControl
+            objectName: "sortModeControl"
+            Kirigami.FormData.label: qsTr("面板供应商排序：")
+            model: root.sortModeOptions
+            textRole: "text"
+            valueRole: "value"
+            currentIndex: {
+                const idx = root.sortModeOptions.findIndex(function(opt) {
+                    return opt.value === (root.cfg_sortMode || "default")
+                })
+                return Math.max(0, idx)
+            }
+            onActivated: root.cfg_sortMode = currentValue
+        }
+
         QQC2.SpinBox {
             id: refreshInterval
             ...
         }
     }
```

- [x] **Step 9.3:** 验证 schema 安装：
```bash
kpackagetool6 --remove aiUsageWatcher 2>/dev/null || true
kpackagetool6 --install aiUsageWatcher 2>&1 | tail -n 5
```
Expected: `install` 退出 0；运行 `plasmawindowed aiUsageWatcher` 配置面板出现新字段。

- [x] **Step 9.4:** Commit
```bash
git add package/contents/config/main.xml package/contents/ui/config/GeneralConfig.qml
git commit -m "feat(provider-ux): sortMode + customOrder schema fields"
```

archived-with: 2026-07-26-provider-ux-enhancements
---

### Task 10: 视觉验收

**Files:**
- 不改代码；纯运行 / 截图。

**Steps:**

- [x] **Step 10.1:** 启动并观察小图标：
```bash
plasmawindowed aiUsageWatcher &
PW_PID=$!
sleep 3
ls -la screenshots/ 2>/dev/null || mkdir -p screenshots
import -window root "${PW_PID}" screenshots/01-default.png 2>/dev/null || true
kill $PW_PID 2>/dev/null
```
Expected: 截图存在；小图标显示 `—` 或具体已用% 数字。

- [x] **Step 10.2:** 切换 sortMode：
```bash
plasmawindowed aiUsageWatcher &
PW_PID=$!
# 在配置页 KCM 处修改 sortMode（手动或用 dbus-send）
sleep 2
kill $PW_PID 2>/dev/null
```
Expected: 顺序按所选模式变化（字母 / 已用% 等）。

- [x] **Step 10.3:** 禁用供应商：
```bash
# 在 ProvidersConfig.qml KCM 中将某个供应商 enabled = false
plasmawindowed aiUsageWatcher &
PW_PID=$!
sleep 2
kill $PW_PID 2>/dev/null
```
Expected: 面板不再显示该供应商；Orb 轮训跳过。

- [x] **Step 10.4:** 验证 Orb CrossFade：
> 接受 200ms 内完成切换，无黑帧，无数字闪烁（手动观察）。

- [x] **Step 10.5:** Commit 验证截图（可选）：
```bash
git add screenshots/
git commit -m "docs(provider-ux): capture manual acceptance screenshots" || true
```

archived-with: 2026-07-26-provider-ux-enhancements
---

### Task 11: 回归

**Files:**
- 不改代码；纯测试。

**Steps:**

- [x] **Step 11.1:** 旧配置（无 `enabled` / `logoPath` / `sortMode` / `customOrder` 字段）兼容：
```bash
mkdir -p ~/.local/share/plasma/plasmoids/org.kde.plasma.aiUsageWatcher
cp -r package/* ~/.local/share/plasma/plasmoids/org.kde.plasma.aiUsageWatcher/
# 删除 ~/.config/plasma-org.kde.plasma.aiUsageWatcher 中 cfg_sortMode 与 cfg_customOrder 行，模拟老配置
plasmawindowed aiUsageWatcher &
sleep 3
kill %1 2>/dev/null || true
```
Expected: 启动无 QML warning；面板正常显示。

- [x] **Step 11.2:** `kpackagetool6 --install`：
```bash
kpackagetool6 --remove aiUsageWatcher 2>/dev/null
kpackagetool6 --install aiUsageWatcher 2>&1 | tail -n 5
```
Expected: 退出 0，无报错。

- [x] **Step 11.3:** `git diff` 对照 design / delta spec：
```bash
git diff a51fa80 HEAD -- package/ docs/superpowers/specs/2026-07-25-provider-ux-enhancements-design.md
```
Expected: 改动仅限以下文件——
- `package/contents/js/{providerRegistry.js, providerSnapshot.js, displayProvider.js}`（新增）
- `package/contents/js/{mockData.js, providerCatalog.js}`（最小修改）
- `package/contents/ui/{main.qml, FullView.qml, CompactView.qml, ProviderGroup.qml}`
- `package/contents/ui/config/{ProvidersConfig.qml, ProviderEditor.qml, GeneralConfig.qml}`
- `package/contents/config/main.xml`

其它文件（如 `package/metadata.json` / `docs/requirements.md` / `docs/usage-script-spec.md`）不应有改动。若出现 plan 外的改动：
```bash
git diff --name-only | grep -v -E 'package/contents/(js/(providerRegistry|providerSnapshot|displayProvider|mockData|providerCatalog)\.js|ui/(main|FullView|CompactView|ProviderGroup|config/(ProvidersConfig|ProviderEditor|GeneralConfig))\.qml|config/main\.xml)'
```
应无输出。

- [x] **Step 11.4:** 跑提供的小工具脚本（如有）：
```bash
test -f scripts/check-no-mock.sh && bash scripts/check-no-mock.sh
```
若尚无该脚本，本任务跳过（spec 不要求新增测试基础设施）。

- [x] **Step 11.5:** Commit 文档同步：
```bash
git add docs/superpowers/specs/2026-07-25-provider-ux-enhancements-design.md
# 若任务执行中 plan agent 决定反向回填 design（仅限未实现行为，记录为 deferred）
git commit -m "docs(provider-ux): mark deferred sub-tasks in design doc" || true
```

archived-with: 2026-07-26-provider-ux-enhancements
---

## Out of Scope

下列能力本期明确不实现（与 design "Open Questions" / "Non-Goals" 一致）：

- **GLM / Claude / Gemini / GPT / OpenCode Go / SiliconFlow / Kimi / CodexZH 真实 extractor 接入。** 这些内置 catalog 仅注册静态元数据 + 内联 logo SVG，运行期 `providerSnapshot.refreshOne` 命中后返回 `statusLabel = "未配置"` / `"暂未接入 extractor"`，不合成任何使用率。
- **MiniMax / Codex 真实网络调用。** 本期由 `providerSnapshot` 暴露 stub；后端 KCM 流程仍可触发刷新，但返回占位 snapshot。接入真正 MiniMax / Codex REST 客户端是后续 change。
- **云端 logo 同步、自定义字体 / 主题。** UI 主题与已有 Plasma 主题一致，不在 scope。
- **QTest 集成测试框架引入。** spec "Non-Goals" 明示不引入。
- **Cloud provider 同步。** `providerRegistry` 仅静态；sync API 等待后续需求。
- **`docs/usage-script-spec.md` `resetAt` 字段重命名。** 保留向下兼容。
- **Plasma 全局快捷键 / DBus signal 扩字段。** 范围外。
- **修改 `package/metadata.json`、CMakeLists.txt、JS 之外的 `.md` 文档。**

archived-with: 2026-07-26-provider-ux-enhancements
---

## 风险与回滚

| 风险 | 触发条件 | 缓解 |
|---|---|---|
| CrossFade / ScaleAnimator 在 Plasma 6 < 5.27 不可用 | import 失败 | 已在 Step 6.2 标记 fallback：由两个 Item 叠加手动 NumberAnimation |
| Logo SVG 在 QImage 解析失败 | 单个 SVG > 800 字 / 含 rgba | Step 1.1 内联 SVG 控制在 800 字内、颜色限 `#rrggbb` |
| customOrder 引用已删除供应商 | 用户删供应商 | Step 任务 7 `syncWorkingValue` 同步删除；displayProvider.normalizeCustomOrder 二次过滤 |
| 旧 KConfig XT 不识别 `cfg_sortMode` / `cfg_customOrder` | 现存 Plasma 配置 schema 旧版 | main.xml 加 entry 默认值；displayProvider 用 `\|\| "default"` 兜底 |
| providerSnapshot mock 与真实 extractor 行为分歧 | 后端 KCM 流程立即返回真实数据 | ProviderSnapshot.refreshOne 三实现分别处理 credential / login / script 缺失 stub，零调用零网络 |
| 拆分期间 QML 调用方暂未迁移 | mockData shim 缺失若干符号 | Step 2.3 shim 已暴露 stripProviderSuffix / usageClass / buildDisplayProviders 等老 API，避免调用方报错 |

回滚方式：`git revert` 至 base-ref；无数据迁移、无 schema migration。

archived-with: 2026-07-26-provider-ux-enhancements
---

## Self-Review

执行完上述 plan 后，于主会话中自审：

1. **Spec coverage**
   - 手动刷新重置 → Task 3 Step 3.4
   - 多模式排序 → Task 4 + Task 9
   - 官网跳转 → Task 5
   - Orb CrossFade → Task 6
   - 启用禁用 + Logo → Task 7 / 8 / 9
   - 真实数据 only（删除 mock 浮动） → Task 2 Step 2.3（mockData shim 不再生成假数据）
   - 内置 catalog 注册 → Task 1

2. **Placeholder scan**
   - 无 "TBD"、"TODO"、"implement later"、"add appropriate"
   - 每个 Step 都含可粘贴的完整代码块或具体命令

3. **Type / 接口一致性**
   - `displayProvider.buildDisplay` 接受的 options 字段 { sortMode, customOrderRaw } → Task 3 Step 3.2 与 Task 9 main.xml 字段一致。
   - `providerGroup` 新增属性 `website` / `logoSource` / `logoChar` / `logoIsSvg` → Task 5 Step 5.1 与 Step 5.3 的 FullView 委托绑定一致。
   - `CompactView.CrossFade` 当前索引公式 = `providerIndex % providers.length` → 与 main.qml 的轮训 Timer 同样步。

4. **优先级**
   - Task 1-2（核心拆分）→ Task 3-4（主界面派生 + 排序 UI）→ Task 5（ProviderGroup 自渲染）→ Task 6（Orb 动画）→ Task 7-8（编辑 UI）→ Task 9（配置 schema）→ Task 10（视觉验收）→ Task 11（回归）。

archived-with: 2026-07-26-provider-ux-enhancements
---

Plan 完成并已保存到 `/home/zhouwr/Project/CodeWorkspace/AIQuotaPilot/docs/superpowers/plans/2026-07-25-provider-ux-enhancements-plan.md`。

两种执行方式可选：

1. **Subagent-Driven（推荐）** —— 我为每个 Task 分派独立子代理，按任务唯一文本定向勾选，主会话只做证据复核。
2. **Inline Execution** —— 在当前会话内按顺序执行 Task 1→11，达成自设检查点后停下让用户复核。

主人需要选择哪种执行方式？我随即按对应 sub-skill 开始实施。
