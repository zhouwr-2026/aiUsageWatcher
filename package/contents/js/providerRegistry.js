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
