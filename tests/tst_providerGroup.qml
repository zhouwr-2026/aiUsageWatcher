import QtQuick
import QtQuick.Controls as QQC2
import QtTest
import "../package/contents/ui"

Item {
    width: 420
    height: 180

    ProviderGroup {
        id: providerGroup

        objectName: "providerGroup"
        width: parent.width
        providerName: "示例供应商"
        statusLabel: "可用"
        ledClass: "led-green"
        logoSource: "data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='24' height='24'><rect width='24' height='24' fill='red'/></svg>"
        logoChar: "示"
        logoIsSvg: false
        plans: [{
            planName: "7 天",
            usedPercent: 50,
            usedPercentLabel: "50%",
            barClass: "bar-green",
            usedText: "50",
            totalText: "100",
            unitText: "USD",
            resetText: "明天 10:00",
            extraText: "今日 222次/$31.92 / 51.73Mtoken | 周限 $255.00 / 剩 $223.08"
        }]
    }

    TestCase {
        name: "ProviderGroup"
        when: windowShown

        function test_transparent_status_dot_layout() {
            compare(providerGroup.color.a, 0)

            const indicator = findChild(providerGroup, "providerStatusIndicator")
            const logo = findChild(providerGroup, "providerLogoImage")
            const badge = findChild(providerGroup, "providerStatusBadge")
            const status = findChild(providerGroup, "providerStatusLabel")
            const plan = findChild(providerGroup, "planBar")
            const extraTextLabel = findChild(providerGroup, "extraTextLabel")
            verify(indicator !== null)
            verify(indicator.width > 0)
            compare(indicator.width, indicator.height)
            compare(indicator.color.a, 0)
            providerGroup.providerName = "Codex"
            compare(indicator.color, Qt.rgba(1, 1, 1, 1))
            compare(indicator.border.width, 1)
            providerGroup.providerName = "MiniMax"
            compare(indicator.color.a, 0)
            compare(indicator.border.width, 0)
            providerGroup.providerName = "CodexZH"
            compare(indicator.color.a, 0)
            compare(indicator.border.width, 0)
            providerGroup.logoBackdropColor = "#b6c0cc"
            compare(indicator.color, Qt.color("#b6c0cc"))
            compare(indicator.border.width, 1)
            compare(logo.anchors.margins, Math.round(indicator.width * 0.1))
            providerGroup.logoBackdropColor = ""
            verify(logo !== null)
            verify(logo.source.toString().indexOf("data:image/svg+xml;utf8,") === 0)
            verify(badge !== null)
            verify(status !== null)
            compare(status.text, "可用")
            verify(plan !== null)
            const progress = findChild(plan, "planProgressBar")
            verify(progress !== null)
            compare(progress.from, 0)
            compare(progress.to, 100)
            compare(progress.value, 50)
            verify(findChild(progress, "unusedTrack").width > 0)
            verify(extraTextLabel !== null)
            compare(extraTextLabel.text, plan.fullDetailText)
            verify(plan.fullDetailTooltipText.indexOf("\n") > 0)
        }

        function test_extraTextTooltip_text_equals_extraText_lines() {
            const plan = findChild(providerGroup, "planBar")
            const tooltip = findChild(plan, "extraTextTooltip")
            verify(tooltip !== null)
            const expected = plan.extraText.split(" | ").join("\n")
            compare(tooltip.text, expected)
        }

        function test_extraTextTooltip_has_no_USD_prefix() {
            const extraTextLabel = findChild(providerGroup, "extraTextLabel")
            const tooltip = findChild(extraTextLabel, "extraTextTooltip")
            verify(extraTextLabel !== null)
            verify(tooltip !== null)
            verify(!tooltip.text.startsWith("USD"))
            verify(!tooltip.text.startsWith("USD ·"))
        }

        function test_extraTextTooltip_sixteen_lines_for_codexzh() {
            providerGroup.plans = [{
                planName: "周限额",
                usedPercent: 50,
                usedPercentLabel: "50%",
                barClass: "bar-green",
                usedText: "30",
                totalText: "255",
                unitText: "USD",
                resetText: "明天",
                extraText: "今日调用：204 | 今日消费：$30.02 | 今日 Token：49,782,650 | 日限额度：$255.00 | 今日剩余：$224.98 | 本周调用：204 | 本周消费：$30.02 | 周限额度：$255.00 | 实时剩余：$224.98 | 总请求次数：1,960 | 总使用额度：$305.10 | 总使用 Token：446,034,226 | RPM：0 | TPM：0 | 订阅开始：2026-03-03 10:23:40 | 订阅到期：2026-08-22 17:23:53"
            }]
            const extraTextLabel = findChild(providerGroup, "extraTextLabel")
            const tooltip = findChild(extraTextLabel, "extraTextTooltip")
            verify(extraTextLabel !== null)
            verify(tooltip !== null)
            const lines = tooltip.text.split("\n")
            compare(lines.length, 16)
            compare(lines[0], "今日调用：204")
            verify(!tooltip.text.startsWith("USD ·"))
        }

        function test_extraTextTooltip_y_is_below_label() {
            const extraTextLabel = findChild(providerGroup, "extraTextLabel")
            const tooltip = findChild(extraTextLabel, "extraTextTooltip")
            verify(extraTextLabel !== null)
            verify(tooltip !== null)
            verify(tooltip.preferredY > extraTextLabel.height)
            compare(tooltip.popupType, QQC2.Popup.Window)
        }

        function test_unused_track_fills_progress_bar() {
            const progress = findChild(providerGroup, "planProgressBar")
            const track = findChild(progress, "unusedTrack")
            verify(track !== null)
            compare(track.opacity, 1)
            compare(track.width, progress.width)
            verify(track.height > 0)
            compare(track.height,
                    progress.height - progress.topInset - progress.bottomInset)
        }

        function test_planName_does_not_repeat_limit() {
            const plan = findChild(providerGroup, "planBar")
            compare(plan.renderTemplate("%1 限额 %2/%3",
                                        ["周限额", "36", "255"]),
                    "周限额 36/255")
            compare(plan.renderTemplate("%1 限额 %2/%3",
                                        ["7 天", "36", "100"]),
                    "7 天 限额 36/100")
        }

        function test_plan_progress_clamps_boundary_values() {
            const plan = findChild(providerGroup, "planBar")
            const progress = findChild(plan, "planProgressBar")
            const fill = findChild(progress, "planProgressFill")
            verify(fill !== null)

            // Normal value 50 with bar-green: fill color matches usageColor(bar-green).
            plan.usedPercent = 50
            plan.usedPercentLabel = "50%"
            plan.barClass = "bar-green"
            compare(progress.value, 50)
            compare(fill.color, plan.usageColor("bar-green"))
            verify(findChild(progress, "unusedTrack").width > 0)

            plan.usedPercent = 0
            plan.usedPercentLabel = "0%"
            plan.barClass = "bar-green"
            compare(progress.value, 0)
            verify(findChild(progress, "unusedTrack").width > 0)

            plan.usedPercent = 100
            plan.usedPercentLabel = "100%"
            plan.barClass = "bar-red"
            compare(progress.value, 100)
            verify(findChild(progress, "unusedTrack").width > 0)

            // No-data: caller supplies "—" and a disabled bar class; label and fill follow.
            plan.usedPercent = -1
            plan.usedPercentLabel = "—"
            plan.barClass = "bar-gray"
            compare(progress.value, 0)
            compare(findChild(plan, "planUsedPercentLabel").text, "—")
            compare(fill.color, plan.usageColor("bar-gray"))
            verify(findChild(progress, "unusedTrack").width > 0)

            plan.usedPercent = 50
            plan.usedPercentLabel = "50%"
            plan.barClass = "bar-green"
        }
    }
}
