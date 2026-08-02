// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library
.import "providerNormalize.js" as ProviderNormalize
.import "providerCatalog.js" as ProviderCatalog
.import "scriptTools.js" as ScriptTools

function parseWorkingDefinitions(json) {
    return ProviderNormalize.normalizeDefinitions(json)
}

function serializeDefinitions(items) {
    return JSON.stringify(items)
}

function validateProvider(candidate, siblings) {
    if (!candidate || typeof candidate !== "object")
        return { valid: false, message: "供应商定义无效" }

    var id = typeof candidate.id === "string" ? candidate.id.trim() : ""
    if (!id)
        return { valid: false, message: "供应商 ID 不能为空" }
    if (typeof candidate.providerName !== "string" || !candidate.providerName.trim())
        return { valid: false, message: "供应商名称不能为空" }

    siblings = Array.isArray(siblings) ? siblings : []
    for (var i = 0; i < siblings.length; ++i) {
        var siblingId = siblings[i] && typeof siblings[i].id === "string"
            ? siblings[i].id.trim() : ""
        if (siblingId === id)
            return { valid: false, message: "供应商 ID 不能重复" }
    }

    var catalogId = ProviderCatalog.catalogIdForLegacy(candidate)
    if (catalogId !== ProviderCatalog.CUSTOM_ID) {
        var preset = ProviderCatalog.definitionFor(catalogId)
        var candidateForCompare = JSON.parse(JSON.stringify(candidate))
        delete candidateForCompare.price
        delete candidateForCompare.topUpAmount
        delete candidateForCompare.topUpDate
        if (!preset || JSON.stringify(candidateForCompare) !== JSON.stringify(preset))
            return { valid: false, message: "固定厂商信息必须使用内置预设" }
        return { valid: true, message: "" }
    }

    var website = typeof candidate.website === "string" ? candidate.website.trim() : ""
    if (!/^https?:\/\/[^\s]+$/i.test(website))
        return { valid: false, message: "官网链接必须是有效的 HTTP(S) 地址" }

    if (!Array.isArray(candidate.plans) || candidate.plans.length === 0)
        return { valid: false, message: "至少需要一个套餐" }

    var planNames = []
    for (var planIndex = 0; planIndex < candidate.plans.length; ++planIndex) {
        var plan = candidate.plans[planIndex]
        var planName = plan && typeof plan.planName === "string"
            ? plan.planName.trim() : ""
        if (!planName)
            return { valid: false, message: "限额名称不能为空" }
        if (planNames.indexOf(planName) >= 0)
            return { valid: false, message: "限额名称不能重复" }
        if (!ScriptTools.variableName(plan.usedVariable))
            return { valid: false, message: "已用量变量必须使用 ${name} 格式" }
        if (!ScriptTools.variableName(plan.limitVariable))
            return { valid: false, message: "限额总量变量必须使用 ${name} 格式" }
        var resetVariable = typeof plan.resetVariable === "string"
            ? plan.resetVariable.trim() : ""
        if (resetVariable && !ScriptTools.variableName(resetVariable))
            return { valid: false, message: "到期时间变量必须使用 ${name} 格式" }
        planNames.push(planName)
    }

    var price = typeof candidate.price === "number" ? candidate.price : NaN
    if (candidate.price !== undefined && candidate.price !== null
            && (!isFinite(price) || price < 0))
        return { valid: false, message: "套餐价格必须为非负数字或留空" }
    var topUpAmount = typeof candidate.topUpAmount === "number" ? candidate.topUpAmount : NaN
    if (candidate.topUpAmount !== undefined && candidate.topUpAmount !== null
            && (!isFinite(topUpAmount) || topUpAmount < 0))
        return { valid: false, message: "充值金额必须为非负数字或留空" }
    var topUpDate = typeof candidate.topUpDate === "string" ? candidate.topUpDate.trim() : ""
    if (topUpDate) {
        if (!/^\d{4}-\d{2}-\d{2}$/.test(topUpDate))
            return { valid: false, message: "充值时间必须使用 YYYY-MM-DD 格式" }
        var dateMonth = parseInt(topUpDate.substring(5, 7), 10)
        var dateDay = parseInt(topUpDate.substring(8, 10), 10)
        if (dateMonth < 1 || dateMonth > 12 || dateDay < 1 || dateDay > 31)
            return { valid: false, message: "充值时间必须是有效日期" }
    }

    var contract = ScriptTools.validateContract(candidate.script, candidate.plans)
    if (!contract.valid)
        return contract
    return { valid: true, message: "" }
}
