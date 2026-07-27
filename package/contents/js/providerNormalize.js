// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library

.import "displayProvider.js" as DisplayProvider
.import "providerCatalog.js" as ProviderCatalog
.import "scriptTools.js" as ScriptTools

var DEFAULT_TEMPLATE = "%1 限额  %2/%3  重置于 %4";

function _isFiniteNumber(value) {
    return typeof value === "number" && isFinite(value);
}

function _copyDefinitions(definitions) {
    return definitions.map(function(definition) {
        return Object.assign({}, definition, {
            "plans": definition.plans.map(function(plan) {
                return Object.assign({}, plan);
            })
        });
    });
}

function normalizeDefinitions(raw) {
    var definitions = raw;
    if (typeof definitions === "string") {
        try {
            definitions = JSON.parse(definitions);
        } catch (error) {
            return DisplayProvider.filterEnabled([]);
        }
    }
    if (!Array.isArray(definitions))
        return DisplayProvider.filterEnabled([]);

    for (var i = 0; i < definitions.length; ++i) {
        if (!definitions[i] || typeof definitions[i] !== "object"
                || !Array.isArray(definitions[i].plans))
            return DisplayProvider.filterEnabled([]);
    }

    return DisplayProvider.filterEnabled(definitions.map(function(definition, providerIndex) {
        var catalogId = ProviderCatalog.catalogIdForLegacy(definition);
        var fixedDefinition = ProviderCatalog.definitionFor(catalogId);
        if (fixedDefinition) {
            // 内置 catalog 用预设，但保留用户的 enabled / logoPath 状态
            return Object.assign({}, fixedDefinition, {
                "enabled": definition.enabled !== false,
                "logoPath": (typeof definition.logoPath === "string" && definition.logoPath.length > 0)
                    ? definition.logoPath : fixedDefinition.logoPath || ""
            });
        }
        var providerScript = typeof definition.script === "string" && definition.script
            ? definition.script : "";
        if (!providerScript) {
            for (var scriptIndex = 0; scriptIndex < definition.plans.length; ++scriptIndex) {
                if (typeof definition.plans[scriptIndex].script === "string"
                        && definition.plans[scriptIndex].script) {
                    providerScript = definition.plans[scriptIndex].script;
                    break;
                }
            }
        }
        if (!providerScript)
            providerScript = ScriptTools.DEFAULT_SCRIPT;
        return {
            "catalogId": catalogId,
            "id": typeof definition.id === "string" && definition.id
                ? definition.id : "provider-" + (providerIndex + 1),
            "providerName": typeof definition.providerName === "string"
                ? definition.providerName : "",
            "website": typeof definition.website === "string"
                ? definition.website : "",
            "vendor": typeof definition.vendor === "string"
                ? definition.vendor : "",
            "sourceLabel": typeof definition.sourceLabel === "string"
                ? definition.sourceLabel : "",
            "trustMode": typeof definition.trustMode === "string"
                ? definition.trustMode : "strict",
            "template": typeof definition.template === "string" && definition.template
                ? definition.template : DEFAULT_TEMPLATE,
            "script": providerScript,
            "enabled": definition.enabled !== false,
            "logoPath": typeof definition.logoPath === "string" ? definition.logoPath : "",
            "plans": definition.plans.map(function(plan, planIndex) {
                plan = plan && typeof plan === "object" ? plan : {};
                return {
                    "id": typeof plan.id === "string" && plan.id
                        ? plan.id : "plan-" + (planIndex + 1),
                    "planName": typeof plan.planName === "string" ? plan.planName : "",
                    "unit": typeof plan.unit === "string" ? plan.unit : "",
                    "sourceType": plan.sourceType === "manual"
                        ? "manual" : "http-js",
                    "limit": _isFiniteNumber(plan.limit) ? plan.limit : 0,
                    "manualUsed": _isFiniteNumber(plan.manualUsed) ? plan.manualUsed : 0,
                    "requestUrl": typeof plan.requestUrl === "string" ? plan.requestUrl : "",
                    "script": typeof plan.script === "string" ? plan.script : "",
                    "usedVariable": typeof plan.usedVariable === "string" && plan.usedVariable
                        ? plan.usedVariable : "${used}",
                    "limitVariable": typeof plan.limitVariable === "string" && plan.limitVariable
                        ? plan.limitVariable : "${limit}",
                    "resetVariable": typeof plan.resetVariable === "string"
                        ? plan.resetVariable : ""
                };
            })
        };
    }));
}

function _manualPlans(definition) {
    return definition.plans.filter(function(plan) {
        return plan.sourceType === "manual";
    }).map(function(plan) {
        var valid = _isFiniteNumber(plan.manualUsed) && plan.manualUsed >= 0
            && _isFiniteNumber(plan.limit) && plan.limit > 0;
        return {
            "planId": plan.id,
            "planName": plan.planName,
            "used": plan.manualUsed,
            "total": plan.limit,
            "unit": plan.unit,
            "resetText": "",
            "extraText": "",
            "isValid": valid,
            "invalidReason": valid ? "" : "手动用量或限额无效"
        };
    });
}

function createSeedSnapshots(definitions) {
    if (!Array.isArray(definitions))
        return [];

    var snapshots = [];
    definitions.forEach(function(definition) {
        var manualPlans = _manualPlans(definition);
        if (manualPlans.length > 0) {
            snapshots.push({
                "providerId": definition.id,
                "statusLabel": manualPlans.some(function(plan) { return plan.isValid; })
                    ? "可用" : "配置无效",
                "errorText": "",
                "plans": manualPlans
            });
        }
    });
    return snapshots;
}

function replaceSnapshot(snapshots, replacement) {
    snapshots = Array.isArray(snapshots) ? snapshots : [];
    if (!replacement || typeof replacement !== "object")
        return snapshots.slice();

    var found = false;
    var result = snapshots.map(function(snapshot) {
        if (!snapshot || snapshot.providerId !== replacement.providerId)
            return snapshot;
        found = true;
        return replacement;
    });
    if (!found)
        result.push(replacement);
    return result;
}

function stripProviderSuffix(name) {
    if (typeof name !== "string")
        return "";
    var index = name.indexOf(" · ");
    return index >= 0 ? name.substring(0, index) : name;
}

function usageClass(percent, prefix) {
    if (!_isFiniteNumber(percent) || percent < 0)
        return prefix + "-gray";
    if (percent < 85)
        return prefix + "-green";
    if (percent < 95)
        return prefix + "-yellow";
    return prefix + "-red";
}

function providerUsageAt(displayProviders, providerIndex) {
    return DisplayProvider.providerUsageAt(displayProviders, providerIndex);
}

function nextProviderIndexWithUsage(displayProviders, providerIndex) {
    return DisplayProvider.nextProviderIndexWithUsage(displayProviders, providerIndex);
}