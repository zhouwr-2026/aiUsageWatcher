// package/contents/js/mockData.js

.pragma library

.import "providerCatalog.js" as ProviderCatalog
.import "scriptTools.js" as ScriptTools

var DEFAULT_TEMPLATE = "%1 限额  %2/%3  重置于 %4";

var SEED_PROVIDER_DEFINITIONS = [{
    "catalogId": "custom",
    "id": "token-hub",
    "providerName": "云之声Token Hub",
    "website": "https://example.com/",
    "vendor": "自定义",
    "sourceLabel": "自定义",
    "trustMode": "strict",
    "template": DEFAULT_TEMPLATE,
    "script": ScriptTools.DEFAULT_SCRIPT,
    "plans": [{
        "id": "five-hours",
        "planName": "5小时",
        "unit": "",
        "sourceType": "http-js",
        "usedVariable": "${used}",
        "limitVariable": "${limit}"
    }, {
        "id": "seven-days",
        "planName": "7天",
        "unit": "",
        "sourceType": "http-js",
        "usedVariable": "${used}",
        "limitVariable": "${limit}"
    }, {
        "id": "thirty-days",
        "planName": "30天",
        "unit": "",
        "sourceType": "http-js",
        "usedVariable": "${used}",
        "limitVariable": "${limit}"
    }]
}, ProviderCatalog.definitionFor("minimax"), ProviderCatalog.definitionFor("codex")];

var SEED_RUNTIME_SNAPSHOTS = [{
    "providerId": "minimax",
    "statusLabel": "未配置",
    "errorText": "",
    "plans": []
}, {
    "providerId": "codex",
    "statusLabel": "未登录",
    "errorText": "",
    "plans": []
}];

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
            return _copyDefinitions(SEED_PROVIDER_DEFINITIONS);
        }
    }
    if (!Array.isArray(definitions))
        return _copyDefinitions(SEED_PROVIDER_DEFINITIONS);

    for (var i = 0; i < definitions.length; ++i) {
        if (!definitions[i] || typeof definitions[i] !== "object"
                || !Array.isArray(definitions[i].plans))
            return _copyDefinitions(SEED_PROVIDER_DEFINITIONS);
    }

    return definitions.map(function(definition, providerIndex) {
        var catalogId = ProviderCatalog.catalogIdForLegacy(definition);
        var fixedDefinition = ProviderCatalog.definitionFor(catalogId);
        if (fixedDefinition)
            return fixedDefinition;
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
    });
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
        var matchedSnapshot = null;
        for (var i = 0; i < SEED_RUNTIME_SNAPSHOTS.length; ++i) {
            var snapshot = SEED_RUNTIME_SNAPSHOTS[i];
            if (snapshot.providerId === definition.id) {
                matchedSnapshot = Object.assign({}, snapshot, {
                    "plans": snapshot.plans.map(function(plan) {
                        return Object.assign({}, plan);
                    })
                });
                break;
            }
        }
        var manualPlans = _manualPlans(definition);
        if (matchedSnapshot) {
            matchedSnapshot.plans = matchedSnapshot.plans.concat(manualPlans);
            snapshots.push(matchedSnapshot);
        } else if (manualPlans.length > 0) {
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

function _normalizeRuntimeSnapshot(snapshot) {
    if (!snapshot || typeof snapshot !== "object"
            || typeof snapshot.providerId !== "string" || !snapshot.providerId
            || !snapshot.plans || typeof snapshot.plans.length !== "number"
            || snapshot.plans.length < 0)
        return null;

    var plans = [];
    for (var i = 0; i < snapshot.plans.length; ++i) {
        var plan = snapshot.plans[i];
        if (!plan || typeof plan !== "object")
            return null;
        plans.push({
            "planId": plan.planId || "",
            "planName": plan.planName || "",
            "used": plan.used,
            "total": plan.total,
            "unit": plan.unit || "",
            "resetText": plan.resetText || "",
            "extraText": plan.extraText || "",
            "isValid": plan.isValid !== false,
            "invalidReason": plan.invalidReason || ""
        });
    }
    return {
        "providerId": snapshot.providerId,
        "statusLabel": snapshot.statusLabel || "",
        "errorText": snapshot.errorText || "",
        "plans": plans
    };
}

function replaceSnapshot(snapshots, replacement) {
    snapshots = Array.isArray(snapshots) ? snapshots : [];
    replacement = _normalizeRuntimeSnapshot(replacement);
    if (!replacement)
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

function usageClass(percent, prefix) {
    if (!_isFiniteNumber(percent) || percent < 0)
        return prefix + "-gray";
    if (percent < 85)
        return prefix + "-green";
    if (percent < 95)
        return prefix + "-yellow";
    return prefix + "-red";
}

function _snapshotFor(snapshots, providerId) {
    for (var i = 0; i < snapshots.length; ++i) {
        if (snapshots[i].providerId === providerId)
            return snapshots[i];
    }
    return null;
}

function _definitionPlan(definition, planId) {
    for (var i = 0; i < definition.plans.length; ++i) {
        if (definition.plans[i].id === planId)
            return definition.plans[i];
    }
    return null;
}

function _displayPlan(definition, snapshotPlan) {
    var planDefinition = _definitionPlan(definition, snapshotPlan.planId) || {};
    var valid = snapshotPlan.isValid !== false
        && _isFiniteNumber(snapshotPlan.used)
        && _isFiniteNumber(snapshotPlan.total)
        && snapshotPlan.total > 0;
    var percent = valid
        ? Math.max(0, Math.min(100, Math.round(snapshotPlan.used / snapshotPlan.total * 100)))
        : -1;
    var unit = typeof snapshotPlan.unit === "string"
        ? snapshotPlan.unit : (planDefinition.unit || "");
    var compactUnit = unit.length <= 8 && !/\s/.test(unit);

    return {
        "planId": snapshotPlan.planId,
        "planName": snapshotPlan.planName || planDefinition.planName || "",
        "usedPercent": percent,
        "usedPercentLabel": valid ? percent + "%" : "—",
        "usedText": valid ? String(snapshotPlan.used) : "",
        "totalText": valid ? String(snapshotPlan.total) : "",
        "unitText": compactUnit ? unit : "",
        "unitOverflow": compactUnit ? "" : unit,
        "resetText": snapshotPlan.resetText || "",
        "extraText": snapshotPlan.extraText || "",
        "templateText": definition.template,
        "isInvalid": !valid,
        "invalidReason": snapshotPlan.invalidReason || "",
        "barClass": usageClass(percent, "bar")
    };
}

function buildDisplayProviders(definitions, snapshots) {
    if (!Array.isArray(definitions))
        return [];
    snapshots = Array.isArray(snapshots) ? snapshots : [];

    return definitions.map(function(definition) {
        var snapshot = _snapshotFor(snapshots, definition.id);
        var snapshotPlans = snapshot && Array.isArray(snapshot.plans) ? snapshot.plans : [];
        var plans = snapshotPlans.map(function(plan) {
            return _displayPlan(definition, plan);
        });
        var tightest = tightestUsage([{ "plans": plans }]);

        return {
            "id": definition.id,
            "providerName": definition.providerName,
            "vendor": definition.vendor || "",
            "sourceLabel": definition.sourceLabel || "",
            "statusLabel": snapshot ? (snapshot.statusLabel || "") : "暂无用量",
            "errorText": snapshot ? (snapshot.errorText || "") : "",
            "template": definition.template || DEFAULT_TEMPLATE,
            "plans": plans,
            "ledClass": usageClass(tightest.usedPercent, "led")
        };
    });
}

function tightestUsage(displayProviders) {
    var result = { "usedPercent": -1, "providerName": "", "planName": "" };
    if (!Array.isArray(displayProviders))
        return result;

    displayProviders.forEach(function(provider) {
        var plans = provider && Array.isArray(provider.plans) ? provider.plans : [];
        plans.forEach(function(plan) {
            if (_isFiniteNumber(plan.usedPercent) && plan.usedPercent >= 0
                    && plan.usedPercent > result.usedPercent) {
                result = {
                    "usedPercent": plan.usedPercent,
                    "providerName": provider.providerName || "",
                    "planName": plan.planName || ""
                };
            }
        });
    });
    return result;
}

function providerUsageAt(displayProviders, providerIndex) {
    var empty = {
        "usedPercent": -1,
        "providerName": "",
        "planName": "",
        "providerIndex": -1
    };
    if (!Array.isArray(displayProviders) || displayProviders.length === 0)
        return empty;

    var numericIndex = _isFiniteNumber(providerIndex) ? Math.floor(providerIndex) : 0;
    var index = ((numericIndex % displayProviders.length) + displayProviders.length)
        % displayProviders.length;
    var provider = displayProviders[index] || {};
    var usage = tightestUsage([provider]);
    return {
        "usedPercent": usage.usedPercent,
        "providerName": usage.providerName || provider.providerName || "",
        "planName": usage.planName,
        "providerIndex": index,
        "providerId": provider.id || "",
        "statusLabel": provider.statusLabel || "",
        "errorText": provider.errorText || "",
        "plans": Array.isArray(provider.plans) ? provider.plans : []
    };
}

function nextProviderIndexWithUsage(displayProviders, providerIndex) {
    if (!Array.isArray(displayProviders) || displayProviders.length === 0)
        return -1;

    var numericIndex = _isFiniteNumber(providerIndex) ? Math.floor(providerIndex) : 0;
    var currentIndex = ((numericIndex % displayProviders.length)
                        + displayProviders.length) % displayProviders.length;
    return (currentIndex + 1) % displayProviders.length;
}

function stripProviderSuffix(name) {
    if (typeof name !== "string")
        return "";
    var index = name.indexOf(" · ");
    return index >= 0 ? name.substring(0, index) : name;
}
