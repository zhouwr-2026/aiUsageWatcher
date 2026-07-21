// package/contents/js/mockData.js

.pragma library

var DEFAULT_TEMPLATE = "%1 限额  %2/%3  重置于 %4";

var SEED_PROVIDER_DEFINITIONS = [{
    "id": "token-hub",
    "providerName": "云之声Token Hub",
    "sourceLabel": "自定义",
    "trustMode": "strict",
    "template": DEFAULT_TEMPLATE,
    "plans": [{
        "id": "five-hours",
        "planName": "5小时",
        "unit": ""
    }, {
        "id": "seven-days",
        "planName": "7天",
        "unit": ""
    }, {
        "id": "thirty-days",
        "planName": "30天",
        "unit": ""
    }]
}, {
    "id": "minimax",
    "providerName": "MiniMax",
    "sourceLabel": "套餐",
    "trustMode": "strict",
    "template": DEFAULT_TEMPLATE,
    "plans": [{
        "id": "general-interval",
        "planName": "通用模型 · 当前周期",
        "unit": "%"
    }, {
        "id": "general-weekly",
        "planName": "通用模型 · 每周",
        "unit": "%"
    }]
}, {
    "id": "codex",
    "providerName": "Codex",
    "sourceLabel": "订阅",
    "trustMode": "strict",
    "template": DEFAULT_TEMPLATE,
    "plans": [{
        "id": "weekly",
        "planName": "周限额",
        "unit": "次"
    }]
}];

var SEED_RUNTIME_SNAPSHOTS = [{
    "providerId": "token-hub",
    "statusLabel": "可用",
    "errorText": "",
    "plans": [{
        "planId": "five-hours",
        "planName": "5小时",
        "used": 65,
        "total": 100,
        "unit": "",
        "resetText": "今天 18:00",
        "extraText": "",
        "isValid": true,
        "invalidReason": ""
    }, {
        "planId": "seven-days",
        "planName": "7天",
        "used": 22,
        "total": 100,
        "unit": "",
        "resetText": "周日 00:00",
        "extraText": "",
        "isValid": true,
        "invalidReason": ""
    }, {
        "planId": "thirty-days",
        "planName": "30天",
        "used": 8,
        "total": 100,
        "unit": "",
        "resetText": "",
        "extraText": "",
        "isValid": true,
        "invalidReason": ""
    }]
}, {
    "providerId": "minimax",
    "statusLabel": "未配置",
    "errorText": "请在供应商设置中保存 MiniMax API Key",
    "plans": []
}, {
    "providerId": "codex",
    "statusLabel": "可用",
    "errorText": "",
    "plans": [{
        "planId": "weekly",
        "planName": "周限额",
        "used": 503,
        "total": 750,
        "unit": "次",
        "resetText": "周日 00:00",
        "extraText": "",
        "isValid": true,
        "invalidReason": ""
    }]
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
        return {
            "id": typeof definition.id === "string" && definition.id
                ? definition.id : "provider-" + (providerIndex + 1),
            "providerName": typeof definition.providerName === "string"
                ? definition.providerName : "",
            "sourceLabel": typeof definition.sourceLabel === "string"
                ? definition.sourceLabel : "",
            "trustMode": typeof definition.trustMode === "string"
                ? definition.trustMode : "strict",
            "template": typeof definition.template === "string" && definition.template
                ? definition.template : DEFAULT_TEMPLATE,
            "plans": definition.plans.map(function(plan, planIndex) {
                plan = plan && typeof plan === "object" ? plan : {};
                return {
                    "id": typeof plan.id === "string" && plan.id
                        ? plan.id : "plan-" + (planIndex + 1),
                    "planName": typeof plan.planName === "string" ? plan.planName : "",
                    "unit": typeof plan.unit === "string" ? plan.unit : ""
                };
            })
        };
    });
}

function createSeedSnapshots(definitions) {
    if (!Array.isArray(definitions))
        return [];

    var snapshots = [];
    definitions.forEach(function(definition) {
        for (var i = 0; i < SEED_RUNTIME_SNAPSHOTS.length; ++i) {
            var snapshot = SEED_RUNTIME_SNAPSHOTS[i];
            if (snapshot.providerId === definition.id) {
                snapshots.push(Object.assign({}, snapshot, {
                    "plans": snapshot.plans.map(function(plan) {
                        return Object.assign({}, plan);
                    })
                }));
                break;
            }
        }
    });
    return snapshots;
}

function fluctuateSnapshots(snapshots, randomFn) {
    if (!Array.isArray(snapshots))
        return [];
    var random = typeof randomFn === "function" ? randomFn : Math.random;

    return snapshots.map(function(snapshot) {
        var plans = Array.isArray(snapshot.plans) ? snapshot.plans : [];
        if (snapshot.providerId === "minimax") {
            return Object.assign({}, snapshot, {
                "plans": plans.map(function(plan) {
                    return Object.assign({}, plan);
                })
            });
        }
        return Object.assign({}, snapshot, {
            "plans": plans.map(function(plan) {
                if (!_isFiniteNumber(plan.used) || !_isFiniteNumber(plan.total)
                        || plan.total <= 0)
                    return Object.assign({}, plan);
                var delta = (random() * 0.1 - 0.05) * plan.total;
                var used = Math.max(0, Math.min(plan.total, Math.round(plan.used + delta)));
                return Object.assign({}, plan, { "used": used });
            })
        });
    });
}

function replaceSnapshot(snapshots, replacement) {
    snapshots = Array.isArray(snapshots) ? snapshots : [];
    if (!replacement || typeof replacement !== "object"
            || typeof replacement.providerId !== "string"
            || !replacement.providerId
            || !Array.isArray(replacement.plans))
        return snapshots.slice();

    var found = false;
    var result = snapshots.map(function(snapshot) {
        if (!snapshot || snapshot.providerId !== replacement.providerId)
            return snapshot;
        found = true;
        return Object.assign({}, replacement, {
            "plans": replacement.plans.map(function(plan) {
                return Object.assign({}, plan);
            })
        });
    });
    if (!found) {
        result.push(Object.assign({}, replacement, {
            "plans": replacement.plans.map(function(plan) {
                return Object.assign({}, plan);
            })
        }));
    }
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
        "providerIndex": index
    };
}

function stripProviderSuffix(name) {
    if (typeof name !== "string")
        return "";
    var index = name.indexOf(" · ");
    return index >= 0 ? name.substring(0, index) : name;
}
