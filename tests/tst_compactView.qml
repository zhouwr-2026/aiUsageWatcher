import QtQuick
import QtTest
import "../package/contents/ui"

Item {
    id: host
    width: 160
    height: 160

    property var testProviders: []
    property string testStyle: "pie"
    property int testProviderIndex: 0
    property bool testHighlighted: false

    QtObject {
        id: fakePlasmoid
        property bool expanded: false
    }

    CompactView {
        id: compact
        anchors.fill: parent
        providers: host.testProviders
        compactStyle: host.testStyle
        providerIndex: host.testProviderIndex
        highlighted: host.testHighlighted
        plasmoidItem: fakePlasmoid
    }

    TestCase {
        name: "CompactView"
        when: windowShown

        function init() {
            host.testProviders = []
            host.testStyle = "pie"
            host.testProviderIndex = 0
            host.testHighlighted = false
        }

        function test_current_provider_tightest_percent_is_rendered() {
            host.testProviders = [{
                providerName: "A",
                plans: [
                    { planName: "low", usedPercent: 22 },
                    { planName: "high", usedPercent: 88 }
                ]
            }, {
                providerName: "B",
                plans: [{ planName: "mid", usedPercent: 67 }]
            }]

            compare(compact.tightestUsage.usedPercent, 88)
            compare(findChild(compact, "compactPercent").text, "88%")
            const pie = findChild(compact, "compactPie")
            verify(pie.visible)
            compare(compact.boundedPercent, 88)
        }

        function test_empty_data_uses_placeholder() {
            compare(findChild(compact, "compactPercent").text, "—")
        }

        function test_bar_style_is_rendered() {
            host.testProviders = [{
                providerName: "A",
                plans: [{ planName: "quota", usedPercent: 50 }]
            }]
            host.testStyle = "bar"

            verify(findChild(compact, "compactBar").visible)
            compare(findChild(compact, "compactBarPercent").text, "50%")
        }

        function test_click_toggles_popup() {
            const pointerArea = findChild(compact, "compactMouseArea")
            verify(pointerArea !== null)

            compare(fakePlasmoid.expanded, false)
            mouseClick(pointerArea)
            compare(fakePlasmoid.expanded, true)
            mouseClick(pointerArea)
            compare(fakePlasmoid.expanded, false)
        }

        function test_provider_index_rotates_displayed_usage() {
            host.testProviders = [{
                providerName: "MiniMax",
                plans: [{ planName: "每周", usedPercent: 28 }]
            }, {
                providerName: "Codex",
                plans: [{ planName: "每周", usedPercent: 67 }]
            }]

            compare(compact.currentUsage.providerName, "MiniMax")
            compare(findChild(compact, "compactPercent").text, "28%")

            host.testProviderIndex = 1
            compare(compact.currentUsage.providerName, "Codex")
            tryCompare(findChild(compact, "compactPercent"), "text", "67%", 500)
        }

        function test_error_badge_and_event_highlight_are_visible() {
            host.testProviders = [{
                providerName: "Unconfigured",
                statusLabel: "未配置",
                errorText: "",
                plans: []
            }]
            verify(!findChild(compact, "errorBadge").visible)

            host.testProviders = [{
                providerName: "Failed",
                errorText: "请求失败",
                plans: []
            }]
            host.testHighlighted = true

            verify(findChild(compact, "errorBadge").visible)
            compare(compact.highlighted, true)
        }
    }
}
