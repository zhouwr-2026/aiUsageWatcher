import QtQuick
import QtTest
import "../package/contents/ui"
import "../package/contents/js/mockData.js" as MockData

Item {
    id: host

    width: 320
    height: 520

    property string configuredCompactStyle: "pie"
    property var testProviders: MockData.buildDisplayProviders(
                                    MockData.SEED_PROVIDER_DEFINITIONS,
                                    MockData.SEED_RUNTIME_SNAPSHOTS)

    FullView {
        id: fullView

        anchors.fill: parent
        providers: host.testProviders
    }

    SignalSpy {
        id: refreshSpy
        target: fullView
        signalName: "refreshRequested"
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
            host.configuredCompactStyle = "pie"
            host.testProviders = MockData.buildDisplayProviders(
                                    MockData.SEED_PROVIDER_DEFINITIONS,
                                    MockData.SEED_RUNTIME_SNAPSHOTS)
            refreshSpy.clear()
            configureSpy.clear()
            keepOpenSpy.clear()
            wait(0)
        }

        function test_public_interface_and_single_component_tree() {
            verify(fullView.hasOwnProperty("opacityPercent"))
            verify(fullView.hasOwnProperty("keepPanelOpen"))
            verify(fullView.hasOwnProperty("configureRequested"))
            verify(fullView.hasOwnProperty("keepOpenChanged"))
            tryCompare(fullView, "renderedPlanCount", 5)
            compare(descendantsNamed(fullView, "providerGroup").length, 3)
            compare(descendantsNamed(fullView, "planBar").length, 5)

            host.configuredCompactStyle = "bar"
            wait(0)
            compare(descendantsNamed(fullView, "providerGroup").length, 3)
            compare(descendantsNamed(fullView, "planBar").length, 5)
        }

        function test_actions_emit_expected_signals() {
            const refreshButton = findChild(fullView, "refreshButton")
            const configureButton = findChild(fullView, "configureButton")
            const keepOpenButton = findChild(fullView, "keepOpenButton")

            verify(refreshButton !== null)
            verify(configureButton !== null)
            verify(keepOpenButton !== null)

            mouseClick(refreshButton)
            compare(refreshSpy.count, 1)
            mouseClick(configureButton)
            compare(configureSpy.count, 1)
            mouseClick(keepOpenButton)
            compare(keepOpenSpy.count, 1)
            compare(keepOpenSpy.signalArguments[0][0], true)
        }

        function test_template_uses_independent_values() {
            const bars = descendantsNamed(fullView, "planBar")
            compare(bars.length, 5)

            const detail = findChild(bars[4], "templateTextLabel")
            verify(detail !== null)
            compare(detail.text, "周限额 限额  503/750  重置于 周日 00:00")
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
