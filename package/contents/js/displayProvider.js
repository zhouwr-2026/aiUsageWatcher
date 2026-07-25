// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library

.import "providerRegistry.js" as ProviderRegistry

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
