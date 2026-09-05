// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library

.import "providerCatalog.js" as ProviderCatalog

// 展示层的 provider 名称兜底数据：
//   - 内置 provider 的定义（名称/plans/vendor/logo 路径）由 providerCatalog.js
//     统一提供（单一数据源）；
//   - 本文件仅保留 catalog 缺失时的名称查询兜底。
// 历史：本文件曾复制一份 _PRESETS + definitionFor + _LOGO_ASSETS 等
// （与 providerCatalog 完全重复且缺 deepseek），会导致 UI 展示层不一致；
// 已删除，名称查询改走 ProviderCatalog.presetFor。

function providerNameFor(catalogId) {
    var preset = ProviderCatalog.presetFor(catalogId)
    return preset ? preset.label : ""
}

// 以下两个函数恒空：内置 provider 的 logo 路径与网站来自 definition/ProviderCatalog
// （definitionFor 已填充 logoPath/website）。QML 端保持调用是防御式写法——
// 用户自建 provider（catalogId 不在 preset 内）时返回空字符串，由 UI 字符兜底。
function websiteFor(catalogId) {
    return ""
}

function logoSvgFor(catalogId) {
    return ""
}
