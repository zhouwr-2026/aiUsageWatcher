// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library

var CUSTOM_ID = "custom";
var DEFAULT_TEMPLATE = "%1 限额  %2/%3  重置于 %4";

var _LOGO_ASSETS = {
    minimax: "file:///home/zhouwr/.local/share/plasma/plasmoids/org.kde.plasma.AIQuotaPilot/contents/images/providers/minimax.png",
    codex: "file:///home/zhouwr/.local/share/plasma/plasmoids/org.kde.plasma.AIQuotaPilot/contents/images/providers/codex.svg",
    "zhipu-glm": "file:///home/zhouwr/.local/share/plasma/plasmoids/org.kde.plasma.AIQuotaPilot/contents/images/providers/zhipu-glm.svg",
    "claude-code": "file:///home/zhouwr/.local/share/plasma/plasmoids/org.kde.plasma.AIQuotaPilot/contents/images/providers/claude-code.png",
    "kimi-for-coding": "file:///home/zhouwr/.local/share/plasma/plasmoids/org.kde.plasma.AIQuotaPilot/contents/images/providers/kimi-for-coding.png",
    siliconflow: "file:///home/zhouwr/.local/share/plasma/plasmoids/org.kde.plasma.AIQuotaPilot/contents/images/providers/siliconflow.svg",
    codexzh: "file:///home/zhouwr/.local/share/plasma/plasmoids/org.kde.plasma.AIQuotaPilot/contents/images/providers/codexzh-icon.png",
    "opencode-go": "file:///home/zhouwr/.local/share/plasma/plasmoids/org.kde.plasma.AIQuotaPilot/contents/images/providers/opencode-go.svg",
    deepseek: "file:///home/zhouwr/.local/share/plasma/plasmoids/org.kde.plasma.AIQuotaPilot/contents/images/providers/deepseek.svg"
};

var _LOGO_BACKDROP_COLORS = {
    "opencode-go": "#b6c0cc",
    deepseek: "#b6c0cc"
};

var PRESETS = [{
    "catalogId": "codex",
    "label": "Codex",
    "id": "codex",
    "providerName": "Codex",
    "vendor": "OpenAI",
    "website": "https://developers.openai.com/codex/",
    "sourceLabel": "订阅",
    "plans": [{ "id": "weekly", "planName": "周限额", "unit": "次" }]
}, {
    "catalogId": "claude-code",
    "label": "Claude Code",
    "id": "claude-code",
    "providerName": "Claude Code",
    "vendor": "Anthropic",
    "website": "https://www.anthropic.com/claude-code",
    "sourceLabel": "订阅",
    "plans": [
        { "id": "five-hour", "planName": "5 小时", "unit": "%" },
        { "id": "weekly", "planName": "每周", "unit": "%" }
    ]
}, {
    "catalogId": "opencode-go",
    "label": "OpenCode Go",
    "id": "opencode-go",
    "providerName": "OpenCode Go",
    "vendor": "OpenCode",
    "website": "https://opencode.ai/go",
    "sourceLabel": "订阅",
    "plans": [
        { "id": "five-hour", "planName": "5 小时", "unit": "%" },
        { "id": "weekly", "planName": "每周", "unit": "%" },
        { "id": "monthly", "planName": "月度额度", "unit": "%" }
    ]
}, {
    "catalogId": "minimax",
    "label": "MiniMax",
    "id": "minimax",
    "providerName": "MiniMax",
    "vendor": "MiniMax",
    "website": "https://www.minimaxi.com/",
    "sourceLabel": "套餐",
    "plans": [
        { "id": "general-interval", "planName": "当前周期", "unit": "%" },
        { "id": "general-weekly", "planName": "每周", "unit": "%" }
    ]
}, {
    "catalogId": "zhipu-glm",
    "label": "智谱 GLM",
    "id": "zhipu-glm",
    "providerName": "智谱 GLM",
    "vendor": "智谱 AI",
    "website": "https://open.bigmodel.cn/",
    "sourceLabel": "套餐",
    "plans": [
        { "id": "five-hour", "planName": "5 小时", "unit": "%" },
        { "id": "weekly", "planName": "每周", "unit": "%" }
    ]
}, {
    "catalogId": "kimi-for-coding",
    "label": "Kimi For Coding",
    "id": "kimi-for-coding",
    "providerName": "Kimi For Coding",
    "vendor": "月之暗面",
    "website": "https://www.kimi.com/code/",
    "sourceLabel": "套餐",
    "plans": [
        { "id": "five-hour", "planName": "5 小时", "unit": "%" },
        { "id": "weekly", "planName": "每周", "unit": "%" }
    ]
}, {
    "catalogId": "siliconflow",
    "label": "硅基流动",
    "id": "siliconflow",
    "providerName": "硅基流动",
    "vendor": "SiliconFlow",
    "website": "https://siliconflow.cn/",
    "sourceLabel": "余额",
    "plans": [{ "id": "balance", "planName": "账户余额", "unit": "元" }]
}, {
    "catalogId": "codexzh",
    "label": "CodexZH",
    "id": "codexzh",
    "providerName": "CodexZH",
    "vendor": "CodexZH",
    "website": "https://codexzh.com/",
    "sourceLabel": "套餐",
    "template": "%1 限额  %2/%3",
    "plans": [
        { "id": "daily", "planName": "日限额", "unit": "%" },
        { "id": "monthly", "planName": "月限额", "unit": "%" }
    ]
}, {
    "catalogId": "deepseek",
    "label": "DeepSeek",
    "id": "deepseek",
    "providerName": "DeepSeek",
    "vendor": "DeepSeek",
    "website": "https://platform.deepseek.com/",
    "sourceLabel": "余额",
    "template": "%1 限额  %2/%3",
    "plans": [{ "id": "balance", "planName": "账户余额", "unit": "元" }]
}, {
    "catalogId": "agnes-ai",
    "label": "Agnes AI",
    "id": "agnes-ai",
    "providerName": "Agnes AI",
    "vendor": "Agnes AI",
    "website": "https://agnes-ai.com/",
    "sourceLabel": "套餐",
    "template": "%1 限额  %2/%3",
    "plans": [{ "id": "plan", "planName": "套餐额度", "unit": "%" }]
}, {
    "catalogId": "command-code",
    "label": "Command Code",
    "id": "command-code",
    "providerName": "Command Code",
    "vendor": "Command Code",
    "website": "https://commandcode.ai/",
    "sourceLabel": "订阅",
    "template": "%1 限额  %2/%3",
    "plans": [
        { "id": "five-hour", "planName": "5 小时", "unit": "%" },
        { "id": "weekly", "planName": "每周", "unit": "%" },
        { "id": "monthly", "planName": "月度额度", "unit": "%" }
    ]
}];

function _copy(value) {
    return JSON.parse(JSON.stringify(value));
}

function providerOptions() {
    var options = PRESETS.map(function(preset) {
        return { "text": preset.label, "value": preset.catalogId };
    });
    options.push({ "text": "自定义", "value": CUSTOM_ID });
    return options;
}

function defaultDefinitions() {
    return PRESETS.map(function(preset) { return definitionFor(preset.catalogId); });
}

function presetFor(catalogId) {
    for (var i = 0; i < PRESETS.length; ++i) {
        if (PRESETS[i].catalogId === catalogId)
            return _copy(PRESETS[i]);
    }
    return null;
}

function catalogIdForLegacy(definition) {
    var explicitId = definition && typeof definition.catalogId === "string"
        ? definition.catalogId : "";
    if (explicitId === CUSTOM_ID || presetFor(explicitId))
        return explicitId;
    var id = definition && typeof definition.id === "string" ? definition.id : "";
    return id === "codex" || id === "minimax" ? id : CUSTOM_ID;
}

function definitionFor(catalogId) {
    var preset = presetFor(catalogId);
    if (!preset)
        return null;
    var logoAsset = _LOGO_ASSETS[catalogId] || "";
    return {
        "catalogId": preset.catalogId,
        "id": preset.id,
        "providerName": preset.providerName,
        "website": preset.website,
        "vendor": preset.vendor,
        "sourceLabel": preset.sourceLabel,
        "trustMode": "strict",
        "template": preset.template || DEFAULT_TEMPLATE,
        "script": "",
        "logoPath": logoAsset,
        "logoBackdropColor": _LOGO_BACKDROP_COLORS[catalogId] || "",
        "plans": preset.plans.map(function(plan) {
            return Object.assign({}, plan, {
                "sourceType": "native",
                "usedVariable": "",
                "limitVariable": ""
            });
        })
    };
}
