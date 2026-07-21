// SPDX-License-Identifier: GPL-2.0-or-later

.pragma library
.import "mockData.js" as MockData

function parseWorkingDefinitions(json) {
    return MockData.normalizeDefinitions(json)
}

function serializeDefinitions(items) {
    return JSON.stringify(MockData.normalizeDefinitions(items))
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

    if (!Array.isArray(candidate.plans) || candidate.plans.length === 0)
        return { valid: false, message: "至少需要一个套餐" }

    var planIds = []
    var planNames = []
    for (var planIndex = 0; planIndex < candidate.plans.length; ++planIndex) {
        var plan = candidate.plans[planIndex]
        var planId = plan && typeof plan.id === "string" ? plan.id.trim() : ""
        var planName = plan && typeof plan.planName === "string"
            ? plan.planName.trim() : ""
        if (!planId)
            return { valid: false, message: "套餐 ID 不能为空" }
        if (!planName)
            return { valid: false, message: "套餐名称不能为空" }
        if (planIds.indexOf(planId) >= 0)
            return { valid: false, message: "套餐 ID 不能重复" }
        if (planNames.indexOf(planName) >= 0)
            return { valid: false, message: "套餐名称不能重复" }
        planIds.push(planId)
        planNames.push(planName)
    }

    var template = typeof candidate.template === "string" ? candidate.template : ""
    var placeholders = ["%1", "%2", "%3", "%4"]
    for (var placeholderIndex = 0; placeholderIndex < placeholders.length; ++placeholderIndex) {
        if (template.indexOf(placeholders[placeholderIndex]) < 0) {
            return {
                valid: false,
                message: "模板必须包含 " + placeholders[placeholderIndex]
            }
        }
    }
    return { valid: true, message: "" }
}

function previewTemplate(template) {
    var values = ["5小时", "65", "100", "今天 18:00"]
    var preview = typeof template === "string" ? template : ""
    for (var i = 0; i < values.length; ++i)
        preview = preview.split("%" + (i + 1)).join(values[i])
    return preview
}
