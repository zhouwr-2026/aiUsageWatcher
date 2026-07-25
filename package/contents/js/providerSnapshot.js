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
