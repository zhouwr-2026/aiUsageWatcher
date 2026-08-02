import QtQuick
import QtTest
import "../package/contents/js/providerCatalog.js" as ProviderCatalog
import "../package/contents/js/providerNormalize.js" as ProviderNormalize
import "../package/contents/js/displayProvider.js" as DisplayProvider

Item {
    id: host

    width: 320
    height: 520

    function deepSeekDefinition() {
        const def = ProviderCatalog.definitionFor("deepseek")
        def.topUpAmount = 100
        def.topUpDate = "2026-08-01"
        return def
    }

    function buildWith(topUpAmount, remaining, topUpDate) {
        const def = deepSeekDefinition()
        def.topUpAmount = topUpAmount
        def.topUpDate = topUpDate || ""
        const snapshots = [{
            providerId: "deepseek",
            statusLabel: "可用",
            errorText: "",
            plans: [{
                planId: "balance",
                planName: "账户余额",
                used: -1,
                total: -1,
                remaining: remaining,
                unit: "元",
                resetText: "",
                resetAt: 0,
                extraText: "",
                isValid: true,
                invalidReason: ""
            }]
        }]
        return DisplayProvider.buildDisplay([def], snapshots, { sortMode: "default" })[0]
    }

    function planOf(display) {
        return display.plans[0]
    }

    // 昨天日期字符串（YYYY-MM-DD）：保证「自充值以来已用」分支与本地日期无关，
    // 避免固定日期（如 2026-08-01）在当天运行时被误判为「今日已用」而失败。
    function yesterdayText() {
        const d = new Date()
        d.setDate(d.getDate() - 1)
        const month = d.getMonth() + 1
        const day = d.getDate()
        return d.getFullYear() + "-"
            + (month < 10 ? "0" + month : month) + "-"
            + (day < 10 ? "0" + day : day)
    }

    TestCase {
        name: "PaygInference"
        when: windowShown

        function test_usedIsTopUpMinusRemaining() {
            const display = buildWith(100, 87.5, yesterdayText())
            const plan = planOf(display)
            compare(plan.usedPercent, 13)
            compare(plan.usedText, "¥12.50")
            compare(plan.totalText, "¥100.00")
            verify(plan.extraText.indexOf("剩余 ¥87.50") >= 0)
            verify(plan.extraText.indexOf("充值 " + yesterdayText().substring(5)) >= 0)
            verify(plan.extraText.indexOf("自充值以来已用 ¥12.50") >= 0)
        }

        function test_remainingAboveTopUpIsUnconsumed() {
            const display = buildWith(100, 120, yesterdayText())
            const plan = planOf(display)
            compare(plan.usedPercent, 0)
            compare(plan.usedText, "¥0.00")
            verify(plan.extraText.indexOf("本次充值未消耗") >= 0)
        }

        function test_noTopUpShowsBalanceOnly() {
            const display = buildWith(0, 87.5, "")
            const plan = planOf(display)
            compare(plan.usedPercent, -1)
            compare(plan.extraText, "余额 ¥87.50")
        }

        function test_remainingInvalidKeepsGray() {
            const display = buildWith(100, -1, "")
            const plan = planOf(display)
            compare(plan.usedPercent, -1)
            compare(plan.barClass, "bar-gray")
        }
    }

    TestCase {
        name: "TotalPrice"
        when: windowShown

        function test_sumsPriceAndTopUp() {
            const priced = ProviderCatalog.definitionFor("minimax")
            priced.price = 30
            const payg = deepSeekDefinition()          // topUpAmount = 100
            const displayed = DisplayProvider.buildDisplay([priced, payg], [], { sortMode: "default" })
            compare(DisplayProvider.totalPrice(displayed), 130)
        }

        function test_ignoresZeroAndMissing() {
            const priced = ProviderCatalog.definitionFor("codex")   // 无 price
            const zero = ProviderCatalog.definitionFor("minimax")
            zero.price = 0
            const displayed = DisplayProvider.buildDisplay([priced, zero], [], { sortMode: "default" })
            compare(DisplayProvider.totalPrice(displayed), 0)
        }
    }
}