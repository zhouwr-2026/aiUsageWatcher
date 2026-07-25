// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library

// DEPRECATED: use providerRegistry.js / providerSnapshot.js / displayProvider.js
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
    if (!Array.isArray(snapshots))
        snapshots = []
    if (!replacement || typeof replacement !== "object")
        return snapshots.slice()
    var list = snapshots.slice()
    var pid = replacement.providerId
    for (var i = 0; i < list.length; i++) {
        if (list[i] && list[i].providerId === pid) {
            list[i] = replacement
            return list
        }
    }
    list.push(replacement)
    return list
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

function forEach(arr, callback) {
    if (!Array.isArray(arr)) return
    for (var i = 0; i < arr.length; ++i)
        callback(arr[i], i)
}
