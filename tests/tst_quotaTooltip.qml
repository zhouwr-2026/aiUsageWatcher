import QtQuick
import QtTest
import "../package/contents/ui"

Item {
    width: 480
    height: 320

    QuotaTooltip {
        id: tooltip
        provider: ({
            providerName: "MiniMax",
            errorText: "",
            plans: [{
                planName: "5 小时",
                usedText: "25",
                totalText: "100",
                unitText: "次",
                resetText: "今天 18:00",
                usedPercent: 25
            }, {
                planName: "每周",
                usedText: "80",
                totalText: "100",
                unitText: "次",
                resetText: "周一 00:00",
                usedPercent: 80
            }]
        })
    }

    TestCase {
        name: "QuotaTooltip"
        when: windowShown

        function test_only_first_quota_is_described_as_text() {
            compare(tooltip.plans.length, 2)
            compare(tooltip.firstPlan.planName, "5 小时")
            compare(findChild(tooltip, "tooltipTitle").text, "MiniMax · 5 小时")
            const summary = findChild(tooltip, "tooltipUsageSummary").text
            verify(summary.indexOf("25/100 次") >= 0)
            verify(summary.indexOf("今天 18:00") >= 0)
            verify(summary.indexOf("80/100") < 0)
            verify(summary.indexOf("每周") < 0)
        }
    }
}
