// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library

.import "providerCatalog.js" as ProviderCatalog

// 展示层的 provider 视觉/名称兜底数据：
//   - 内置 provider 的定义（名称/plans/vendor/logo 路径）由 providerCatalog.js
//     统一提供（单一数据源）；
//   - 本文件只做「catalog 之外的补充兜底」：内置 logo 资源 URL 与名称查询。
// 历史：本文件曾复制一份 _PRESETS + definitionFor/providerOptions 等
// （与 providerCatalog 完全重复且缺 deepseek），会导致 UI 展示层不一致；
// 已删除，名称查询改走 ProviderCatalog.presetFor。

var DEFAULT_TEMPLATE = "%1 限额  %2/%3  重置于 %4"
var CUSTOM_ID = "custom"

var _LOGO_BASE = "file:///home/zhouwr/.local/share/plasma/plasmoids/org.kde.plasma.AIQuotaPilot/contents/images/providers/"

// 与 providerCatalog.js 的 _LOGO_ASSETS 保持一致（沿用同目录图片资源）。
var _LOGO_ASSETS = {
    minimax: _LOGO_BASE + "minimax.png",
    codex: _LOGO_BASE + "codex.svg",
    "zhipu-glm": _LOGO_BASE + "zhipu-glm.svg",
    "claude-code": _LOGO_BASE + "claude-code.png",
    "kimi-for-coding": _LOGO_BASE + "kimi-for-coding.png",
    siliconflow: _LOGO_BASE + "siliconflow.svg",
    codexzh: _LOGO_BASE + "codexzh-icon.png",
    "opencode-go": _LOGO_BASE + "opencode-go.svg",
    deepseek: _LOGO_BASE + "deepseek.svg"
}

function logoAssetFor(catalogId) {
    return _LOGO_ASSETS[catalogId] || ""
}

function providerNameFor(catalogId) {
    var preset = ProviderCatalog.presetFor(catalogId)
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
