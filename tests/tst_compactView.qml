import QtQuick
import QtTest
import "../package/contents/ui"

Item {
    id: host
    width: 160
    height: 160

    property var testProviders: []
    property string testStyle: "pie"

    QtObject {
        id: fakePlasmoid
        property bool expanded: false
    }

    CompactView {
        id: compact
        anchors.fill: parent
        providers: host.testProviders
        compactStyle: host.testStyle
        plasmoidItem: fakePlasmoid
    }

    TestCase {
        name: "CompactView"
        when: windowShown

        function init() {
            host.testProviders = []
            host.testStyle = "pie"
        }

        function test_tightest_percent_is_rendered() {
            host.testProviders = [{
                providerName: "A",
                plans: [{ planName: "low", usedPercent: 22 }]
            }, {
                providerName: "B",
                plans: [{ planName: "high", usedPercent: 88 }]
            }, {
                providerName: "C",
                plans: [{ planName: "mid", usedPercent: 67 }]
            }]

            compare(compact.tightestUsage.usedPercent, 88)
            compare(findChild(compact, "compactPercent").text, "88%")
            const pie = findChild(compact, "compactPie")
            verify(pie.visible)
            verify(pie.segments !== undefined)
            compare(pie.segments.length, 2)
            compare(pie.segments[0].value, 88)
            compare(pie.segments[1].value, 12)
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
        }
    }
}
