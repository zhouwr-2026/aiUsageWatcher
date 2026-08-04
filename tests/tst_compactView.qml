import QtQuick
import QtQuick.Layouts
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
            host.width = 160
            host.height = 160
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
            const chart = findChild(compact, "compactPieChart")
            const source = findChild(compact, "compactPieValueSource")
            verify(chart !== null)
            verify(source !== null)
            compare(source.value, 88)
            compare(chart.range.from, 0)
            compare(chart.range.to, 100)
            compare(compact.boundedPercent, 88)
        }

        function test_empty_data_uses_placeholder() {
            compare(findChild(compact, "compactPercent").text, "—")
            compare(findChild(compact, "compactPieValueSource").value, 0)
        }

        function test_bar_style_is_rendered() {
            host.testProviders = [{
                providerName: "A",
                plans: [{ planName: "quota", usedPercent: 50 }]
            }]
            host.testStyle = "bar"

            verify(findChild(compact, "compactBar").visible)
            compare(findChild(compact, "compactBarPercent").text, "50%")
            const progress = findChild(compact, "compactProgressBar")
            verify(progress !== null)
            compare(progress.from, 0)
            compare(progress.to, 100)
            compare(progress.value, 50)
        }

        function test_bar_minimum_size_contains_track() {
            host.testProviders = [{
                providerName: "A",
                plans: [{ planName: "quota", usedPercent: 50 }]
            }]
            host.testStyle = "bar"
            host.width = compact.Layout.minimumWidth
            host.height = compact.Layout.minimumHeight
            wait(0)

            const track = findChild(compact, "compactBarTrack")
            verify(track.width > 0)
            verify(track.parent.x + track.parent.width <= compact.width,
                   "track end=" + (track.parent.x + track.parent.width)
                   + ", compact width=" + compact.width)
        }

        function test_native_charts_clamp_boundary_values() {
            host.testProviders = [{
                providerName: "A",
                plans: [{ planName: "quota", usedPercent: 0 }]
            }]
            compare(findChild(compact, "compactPieValueSource").value, 0)

            host.testProviders = [{
                providerName: "A",
                plans: [{ planName: "quota", usedPercent: 100 }]
            }]
            compare(findChild(compact, "compactPieValueSource").value, 100)

            host.testStyle = "bar"
            compare(findChild(compact, "compactProgressBar").value, 100)
            verify(findChild(compact, "compactBarTrack").width > 0)
        }

        function test_pie_value_and_color_at_normal_percent() {
            host.testProviders = [{
                providerName: "A",
                plans: [{ planName: "quota", usedPercent: 50 }]
            }]

            const valueSource = findChild(compact, "compactPieValueSource")
            const colorSource = findChild(compact, "compactPieColorSource")
            verify(valueSource !== null)
            verify(colorSource !== null)
            compare(valueSource.value, 50)
            // The chart wires usageColor() into the colorSource; the
            // value must match what usageColor returns for the same
            // percent without hard-coding any theme color.
            compare(colorSource.value, compact.usageColor(50))
        }

        function test_pie_no_data_has_disabled_color() {
            // providers stays []; compactPercent already shows "—".
            const valueSource = findChild(compact, "compactPieValueSource")
            const colorSource = findChild(compact, "compactPieColorSource")
            compare(valueSource.value, 0)
            compare(findChild(compact, "compactPercent").text, "—")
            // currentUsage.usedPercent is -1 (no data), so usageColor
            // returns the disabled semantic color. The colorSource
            // must take the same path.
            compare(colorSource.value,
                    compact.usageColor(compact.currentUsage.usedPercent))
        }

        function test_bar_value_and_fill_color_at_fifty() {
            host.testProviders = [{
                providerName: "A",
                plans: [{ planName: "quota", usedPercent: 50 }]
            }]
            host.testStyle = "bar"

            const progress = findChild(compact, "compactProgressBar")
            const fill = findChild(compact, "compactProgressFill")
            verify(progress !== null)
            verify(fill !== null)
            compare(progress.value, 50)
            // The fill rectangle is wired to usageColor() with the
            // current percent; its color must equal what usageColor
            // returns without hard-coding a theme color.
            compare(fill.color, compact.usageColor(50))
        }

        function test_compact_bar_track_fills_progress_bar() {
            host.testProviders = [{
                providerName: "A",
                plans: [{ planName: "quota", usedPercent: 50 }]
            }]
            host.testStyle = "bar"

            const progress = findChild(compact, "compactProgressBar")
            const track = findChild(compact, "compactBarTrack")
            verify(progress !== null)
            verify(track !== null)
            compare(track.opacity, 1)
            compare(track.width, progress.width)
            verify(track.height > 0)
            compare(track.height,
                    progress.height - progress.topInset - progress.bottomInset)
        }

        function test_bar_no_data_uses_placeholder_and_track() {
            host.testStyle = "bar"
            const progress = findChild(compact, "compactProgressBar")
            const fill = findChild(compact, "compactProgressFill")
            const track = findChild(compact, "compactBarTrack")
            compare(progress.value, 0)
            compare(findChild(compact, "compactBarPercent").text, "—")
            // The fill must take the same disabled-color path as
            // usageColor() for the no-data percent.
            compare(fill.color,
                    compact.usageColor(compact.currentUsage.usedPercent))
            verify(track !== null)
            verify(track.width > 0)
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
