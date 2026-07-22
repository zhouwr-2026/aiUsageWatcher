import QtQuick
import QtTest
import "../package/contents/ui"
import "../package/contents/js/mockData.js" as MockData

Item {
    id: host

    width: 320
    height: 520

    property string configuredCompactStyle: "bar"
    property var testProviders: MockData.buildDisplayProviders(
                                    MockData.SEED_PROVIDER_DEFINITIONS,
                                    liveSnapshots())

    function liveSnapshots() {
        let snapshots = MockData.replaceSnapshot(MockData.SEED_RUNTIME_SNAPSHOTS, {
            providerId: "minimax",
            statusLabel: "可用",
            errorText: "",
            plans: [{
                planId: "general-interval",
                planName: "5 小时",
                used: 0,
                total: 100,
                unit: "%",
                resetText: "07-21 21:20",
                extraText: "",
                isValid: true,
                invalidReason: ""
            }, {
                planId: "general-weekly",
                planName: "每周",
                used: 28,
                total: 100,
                unit: "%",
                resetText: "07-26 16:00",
                extraText: "",
                isValid: true,
                invalidReason: ""
            }]
        })
        snapshots = MockData.replaceSnapshot(snapshots, {
            providerId: "token-hub",
            statusLabel: "可用",
            errorText: "",
            plans: [{
                planId: "five-hours", planName: "5小时", used: 65, total: 100,
                unit: "", resetText: "今天 18:00", isValid: true
            }, {
                planId: "seven-days", planName: "7天", used: 22, total: 100,
                unit: "", resetText: "周日 00:00", isValid: true
            }, {
                planId: "thirty-days", planName: "30天", used: 8, total: 100,
                unit: "", resetText: "", isValid: true
            }]
        })
        return MockData.replaceSnapshot(snapshots, {
            providerId: "codex",
            statusLabel: "可用",
            errorText: "",
            plans: [{
                planId: "weekly", planName: "周限额", used: 503, total: 750,
                unit: "次", resetText: "周日 00:00", isValid: true
            }]
        })
    }

    FullView {
        id: fullView

        anchors.fill: parent
        providers: host.testProviders
        panelStyle: host.configuredCompactStyle
    }

    SignalSpy {
        id: refreshSpy
        target: fullView
        signalName: "refreshRequested"
    }

    SignalSpy {
        id: closeSpy
        target: fullView
        signalName: "closeRequested"
    }

    SignalSpy {
        id: configureSpy
        target: fullView
        signalName: "configureRequested"
    }

    SignalSpy {
        id: keepOpenSpy
        target: fullView
        signalName: "keepOpenChanged"
    }

    TestCase {
        name: "FullView"
        when: windowShown

        function descendantsNamed(item, objectName) {
            const matches = []
            function visit(candidate) {
                if (!candidate)
                    return
                if (candidate.objectName === objectName)
                    matches.push(candidate)
                const children = candidate.children || []
                for (let i = 0; i < children.length; ++i)
                    visit(children[i])
            }
            visit(item)
            return matches
        }

        function init() {
            host.width = 320
            host.configuredCompactStyle = "bar"
            host.testProviders = MockData.buildDisplayProviders(
                                    MockData.SEED_PROVIDER_DEFINITIONS,
                                    host.liveSnapshots())
            refreshSpy.clear()
            configureSpy.clear()
            keepOpenSpy.clear()
            closeSpy.clear()
            wait(0)
        }

        function test_public_interface_and_single_component_tree() {
            verify(fullView.hasOwnProperty("opacityPercent"))
            verify(fullView.hasOwnProperty("keepPanelOpen"))
            verify(fullView.hasOwnProperty("configureRequested"))
            verify(fullView.hasOwnProperty("keepOpenChanged"))
            tryCompare(fullView, "renderedPlanCount", 6)
            compare(descendantsNamed(fullView, "providerGroup").length, 3)
            compare(descendantsNamed(fullView, "planBar").length, 6)

        }

        function test_actions_emit_expected_signals() {
            const refreshButton = findChild(fullView, "refreshButton")
            const configureButton = findChild(fullView, "configureButton")
            const keepOpenButton = findChild(fullView, "keepOpenButton")
            const closeButton = findChild(fullView, "closeButton")

            verify(refreshButton !== null)
            verify(configureButton !== null)
            verify(keepOpenButton !== null)
            verify(closeButton !== null)

            mouseClick(refreshButton)
            compare(refreshSpy.count, 1)
            mouseClick(configureButton)
            compare(configureSpy.count, 1)
            mouseClick(keepOpenButton)
            compare(keepOpenSpy.count, 1)
            compare(keepOpenSpy.signalArguments[0][0], true)
            mouseClick(closeButton)
            compare(closeSpy.count, 1)
        }


        function test_pie_panel_renders_models_and_windows() {
            host.configuredCompactStyle = "pie"
            wait(0)

            compare(descendantsNamed(fullView, "providerGroup").length, 0)
            compare(descendantsNamed(fullView, "panelPieProvider").length, 3)
            compare(descendantsNamed(fullView, "panelPiePlan").length, 6)
        }

        function test_template_uses_independent_values() {
            const bars = descendantsNamed(fullView, "planBar")
            compare(bars.length, 6)

            const detail = findChild(bars[5], "templateTextLabel")
            verify(detail !== null)
            compare(detail.text, "周限额 限额  503/750  重置于 周日 00:00")
        }

        function test_bar_keeps_visible_unused_track() {
            const bars = descendantsNamed(fullView, "planBar")
            verify(bars.length > 0)
            const track = findChild(bars[0], "unusedTrack")
            const legend = findChild(fullView, "usageLegendLabel")
            verify(track !== null)
            verify(track.width > 0)
            verify(track.opacity > 0)
            verify(legend !== null)
            compare(legend.text, "图表说明：高亮为已使用，灰色为剩余额度")
        }

        function test_header_uses_chinese_product_name() {
            const title = findChild(fullView, "headerTitle")
            verify(title !== null)
            compare(title.text, "额度领航员")
        }

        function test_long_names_do_not_overlap_header_actions() {
            host.testProviders = [{
                id: "long-provider",
                providerName: "这是一个用于验证窄面板布局不会覆盖操作按钮的超长供应商名称",
                sourceLabel: "很长的来源标签",
                statusLabel: "可用",
                errorText: "",
                ledClass: "led-green",
                plans: [{
                    planName: "同样非常长的套餐名称用于验证省略布局",
                    usedPercent: 50,
                    usedPercentLabel: "50%",
                    barClass: "bar-green",
                    usedText: "50",
                    totalText: "100",
                    templateText: "%1 限额  %2/%3  重置于 %4",
                    resetText: "明天",
                    unitText: "次",
                    extraText: ""
                }]
            }]
            wait(0)

            const title = findChild(fullView, "headerTitle")
            const actions = findChild(fullView, "headerActions")
            verify(title !== null)
            verify(actions !== null)
            const titlePosition = title.mapToItem(fullView, 0, 0)
            const actionsPosition = actions.mapToItem(fullView, 0, 0)
            verify(titlePosition.x + title.width <= actionsPosition.x)
            verify(actionsPosition.x + actions.width <= fullView.width)
        }
    }
}
