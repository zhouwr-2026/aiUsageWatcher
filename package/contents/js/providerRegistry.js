// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library

var DEFAULT_TEMPLATE = "%1 限额  %2/%3  重置于 %4"
var CUSTOM_ID = "custom"

var _LOGO_BASE = "file:///home/zhouwr/.local/share/plasma/plasmoids/org.kde.plasma.AIQuotaPilot/contents/images/providers/"
var _LOGO_ASSETS = {
    minimax: _LOGO_BASE + "minimax.png",
    codex: _LOGO_BASE + "codex.svg",
    "zhipu-glm": _LOGO_BASE + "zhipu-glm.svg",
    "claude-code": _LOGO_BASE + "claude-code.png",
    "kimi-for-coding": _LOGO_BASE + "kimi-for-coding.png",
    siliconflow: _LOGO_BASE + "siliconflow.svg",
    codexzh: _LOGO_BASE + "codexzh-icon.png",
    "opencode-go": _LOGO_BASE + "opencode-go.svg"
}

var _LOGO_CHARS = {
    minimax: "M",
    codex: "C",
    "zhipu-glm": "智",
    "claude-code": "C",
    "kimi-for-coding": "K",
    siliconflow: "硅",
    codexzh: "Z",
    "opencode-go": "O"
}

function logoAssetFor(catalogId) {
    return _LOGO_ASSETS[catalogId] || ""
}

function defaultLogoCharFor(catalogId) {
    return _LOGO_CHARS[catalogId] || ""
}

function providerNameFor(catalogId) {
    var preset = presetById(catalogId)
    return preset ? preset.label : ""
}

function websiteFor(catalogId) {
    return ""
}

function logoSvgFor(catalogId) {
    return ""
}

function resetPeriodSecFor(catalogId, planId) {
    return 0
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

function definitionFor(catalogId) {
    var preset = presetById(catalogId)
    if (!preset)
        return null
    var logoAsset = logoAssetFor(catalogId)
    return {
        catalogId: preset.catalogId,
        id: catalogId,
        providerName: preset.label,
        website: "",
        vendor: preset.vendor || "",
        sourceLabel: preset.sourceLabel || "",
        trustMode: "strict",
        template: DEFAULT_TEMPLATE,
        logoPath: logoAsset,
        defaultLogoChar: defaultLogoCharFor(catalogId),
        resetPeriodSec: 0,
        plans: preset.plans.map(_planCopy)
    }
}

function defaultProviders() {
    return _PRESETS.map(function(preset) { return definitionFor(preset.catalogId) })
}
