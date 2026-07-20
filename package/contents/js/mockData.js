// package/contents/js/mockData.js
// 种子供应商数据 + 波动函数，供 main.qml Timer 驱动刷新

.pragma library

var SEED_PROVIDERS = [{
    "providerName": "云之声Token Hub",
    "ledClass": "led-green",
    "sourceLabel": "自定义",
    "statusLabel": "可用",
    "errorText": "",
    "plans": [{
        "planName": "5小时",
        "usedPercent": 65,
        "usedPercentLabel": "65%",
        "barClass": "bar-green",
        "resetText": "今天 18:00",
        "usedText": "141775516 / 180000000",
        "unitText": "",
        "extraText": ""
    }, {
        "planName": "7天",
        "usedPercent": 22,
        "usedPercentLabel": "22%",
        "barClass": "bar-green",
        "resetText": "周日 00:00",
        "usedText": "",
        "unitText": "",
        "extraText": ""
    }, {
        "planName": "30天",
        "usedPercent": 8,
        "usedPercentLabel": "8%",
        "barClass": "bar-green",
        "resetText": "",
        "usedText": "",
        "unitText": "",
        "extraText": ""
    }]
}, {
    "providerName": "MiniMax · Claude",
    "ledClass": "led-yellow",
    "sourceLabel": "套餐",
    "statusLabel": "降级",
    "errorText": "",
    "plans": [{
        "planName": "余额",
        "usedPercent": 88,
        "usedPercentLabel": "88%",
        "barClass": "bar-yellow",
        "resetText": "",
        "usedText": "12.5 / 100",
        "unitText": "$",
        "extraText": "活动期 8 月底结束"
    }]
}, {
    "providerName": "Codex",
    "ledClass": "led-green",
    "sourceLabel": "订阅",
    "statusLabel": "可用",
    "errorText": "",
    "plans": [{
        "planName": "周限额",
        "usedPercent": 67,
        "usedPercentLabel": "67%",
        "barClass": "bar-green",
        "resetText": "周日 00:00",
        "usedText": "503/750 次",
        "unitText": "",
        "extraText": ""
    }]
}];

// 去除供应商名中的 " · xxx" 后缀（如 "MiniMax · Claude" → "MiniMax"）
function stripProviderSuffix(name) {
    if (typeof name !== "string")
        return "";

    const idx = name.indexOf(" · ");
    return idx >= 0 ? name.substring(0, idx) : name;
}

// 根据 usedPercent 计算 barClass
function _barClass(pct) {
    if (pct <= 5)
        return "bar-red";

    if (pct <= 15)
        return "bar-yellow";

    return "bar-green";
}

// 根据 plans 中最紧张值计算 ledClass
function _ledClass(plans) {
    let worst = -1;
    for (const plan of plans) {
        if (typeof plan.usedPercent === "number" && plan.usedPercent > worst)
            worst = plan.usedPercent;

    }
    if (worst < 0)
        return "led-gray";

    if (worst <= 5)
        return "led-red";

    if (worst <= 15)
        return "led-yellow";

    return "led-green";
}

// 对 providers 数组做不可变波动：每个 plan 的 usedPercent ± random(0-5)
// 返回新数组以触发 QML 绑定
function fluctuateProviders(providers) {
    return providers.map(function (p) {
        const newPlans = p.plans.map(function (plan) {
            const delta = Math.random() * 10 - 5; // -5 ~ +5
            const newPct = Math.max(0, Math.min(100, Math.round(plan.usedPercent + delta)));
            return Object.assign({}, plan, {
                "usedPercent": newPct,
                "usedPercentLabel": newPct + "%",
                "barClass": _barClass(newPct)
            });
        });
        return Object.assign({}, p, {
            "plans": newPlans,
            "ledClass": _ledClass(newPlans)
        });
    });
}
