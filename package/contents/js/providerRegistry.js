// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library

var DEFAULT_TEMPLATE = "%1 限额  %2/%3  重置于 %4"
var CUSTOM_ID = "custom"

var _SVG_DEFAULTS = {
    minimax: {
        providerName: "MiniMax",
        website: "https://www.minimaxi.com/",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<defs><linearGradient id='g-minimax' x1='0' y1='0' x2='1' y2='1'>" +
            "<stop offset='0' stop-color='#ff8a5b'/><stop offset='1' stop-color='#ff5b6c'/>" +
            "</linearGradient></defs>" +
            "<circle cx='12' cy='12' r='10' fill='url(%23g-minimax)'/>" +
            "<path d='M9 8 L15 8 L9 16 L15 16' stroke='white' stroke-width='2' " +
            "fill='none' stroke-linecap='round' stroke-linejoin='round'/>" +
            "</svg>",
        defaultLogoChar: "M",
        resetPeriodSec: 5 * 3600
    },
    codex: {
        providerName: "Codex",
        website: "https://developers.openai.com/codex/",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<defs><linearGradient id='g-codex' x1='0' y1='0' x2='0' y2='1'>" +
            "<stop offset='0' stop-color='#10a37f'/><stop offset='1' stop-color='#0a7a5c'/>" +
            "</linearGradient></defs>" +
            "<rect x='3' y='3' width='18' height='18' rx='4' fill='url(%23g-codex)'/>" +
            "<path d='M8 12 L11 15 L16 9' stroke='white' stroke-width='2' " +
            "fill='none' stroke-linecap='round' stroke-linejoin='round'/>" +
            "</svg>",
        defaultLogoChar: "C",
        resetPeriodSec: 7 * 24 * 3600
    },
    "zhipu-glm": {
        providerName: "智谱 GLM",
        website: "https://open.bigmodel.cn/",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<defs><linearGradient id='g-zhipu-glm' x1='0' y1='0' x2='1' y2='1'>" +
            "<stop offset='0' stop-color='#5b7cff'/><stop offset='1' stop-color='#3859ff'/>" +
            "</linearGradient></defs>" +
            "<circle cx='12' cy='12' r='10' fill='url(%23g-zhipu-glm)'/>" +
            "<path d='M12 6 L12 18 M6 12 L18 12' stroke='white' stroke-width='2' " +
            "stroke-linecap='round'/>" +
            "</svg>",
        defaultLogoChar: "智",
        resetPeriodSec: 5 * 3600
    },
    "claude-code": {
        providerName: "Claude Code",
        website: "https://claude.com/product/claude-code",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<defs><linearGradient id='g-claude-code' x1='0' y1='0' x2='1' y2='1'>" +
            "<stop offset='0' stop-color='#e89a7c'/><stop offset='1' stop-color='#cc785c'/>" +
            "</linearGradient></defs>" +
            "<circle cx='12' cy='12' r='10' fill='url(%23g-claude-code)'/>" +
            "<circle cx='12' cy='12' r='4' fill='none' stroke='white' stroke-width='2'/>" +
            "</svg>",
        defaultLogoChar: "C",
        resetPeriodSec: 5 * 3600
    },
    "kimi-for-coding": {
        providerName: "Kimi For Coding",
        website: "https://www.kimi.com/",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<defs><linearGradient id='g-kimi-for-coding' x1='0' y1='0' x2='1' y2='0'>" +
            "<stop offset='0' stop-color='#2a2a2a'/><stop offset='1' stop-color='#000000'/>" +
            "</linearGradient></defs>" +
            "<rect x='3' y='3' width='18' height='18' rx='4' fill='url(%23g-kimi-for-coding)'/>" +
            "<path d='M8 8 L16 16 M16 8 L8 16' stroke='white' stroke-width='2' " +
            "stroke-linecap='round'/>" +
            "</svg>",
        defaultLogoChar: "K",
        resetPeriodSec: 5 * 3600
    },
    siliconflow: {
        providerName: "硅基流动",
        website: "https://siliconflow.cn/",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<defs><linearGradient id='g-siliconflow' x1='0' y1='0' x2='1' y2='1'>" +
            "<stop offset='0' stop-color='#9d7cff'/><stop offset='1' stop-color='#7c4dff'/>" +
            "</linearGradient></defs>" +
            "<circle cx='12' cy='12' r='10' fill='url(%23g-siliconflow)'/>" +
            "<circle cx='12' cy='12' r='3' fill='white'/>" +
            "</svg>",
        defaultLogoChar: "硅",
        resetPeriodSec: 30 * 24 * 3600
    },
    codexzh: {
        providerName: "CodexZH",
        website: "https://codexzh.com/",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<defs><linearGradient id='g-codexzh' x1='0' y1='0' x2='1' y2='1'>" +
            "<stop offset='0' stop-color='#1fc98c'/><stop offset='1' stop-color='#0f9b6e'/>" +
            "</linearGradient></defs>" +
            "<circle cx='12' cy='12' r='10' fill='url(%23g-codexzh)'/>" +
            "<path d='M12 6 L16 12 L12 18 L8 12 Z' fill='white'/>" +
            "</svg>",
        defaultLogoChar: "Z",
        resetPeriodSec: 7 * 24 * 3600
    },
    "opencode-go": {
        providerName: "OpenCode Go",
        website: "https://opencode.ai/go",
        logoSvg: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>" +
            "<defs><linearGradient id='g-opencode-go' x1='0' y1='0' x2='0' y2='1'>" +
            "<stop offset='0' stop-color='#3385ff'/><stop offset='1' stop-color='#0052cc'/>" +
            "</linearGradient></defs>" +
            "<circle cx='12' cy='12' r='10' fill='url(%23g-opencode-go)'/>" +
            "<path d='M8 9 L8 15 M8 9 L13 9 L13 12 L11 12 L11 15 L8 15' " +
            "stroke='white' stroke-width='2' fill='none' stroke-linecap='round' " +
            "stroke-linejoin='round'/>" +
            "</svg>",
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
        { id: "weekly", planName: "周限额", unit: "%" }
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
