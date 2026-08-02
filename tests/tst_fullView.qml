import QtQuick
import QtTest
import "../package/contents/ui"
import "../package/contents/js/providerNormalize.js" as ProviderNormalize
import "../package/contents/js/displayProvider.js" as DisplayProvider
import "../package/contents/js/providerCatalog.js" as ProviderCatalog

Item {
    id: host

    width: 320
    height: 520

    property string configuredCompactStyle: "bar"

    function tokenHubDefinitions() {
        return [
            ProviderCatalog.definitionFor("minimax"),
            {
                id: "token-hub",
                providerName: "Token Hub",
                template: "%1 限额  %2/%3  重置于 %4",
                plans: []
            },
            ProviderCatalog.definitionFor("codex")
        ]
    }

    property var testProviders: DisplayProvider.buildDisplay(
                                    tokenHubDefinitions(),
                                    liveSnapshots())

    function liveSnapshots() {
        let snapshots = []
        snapshots = ProviderNormalize.replaceSnapshot([], {
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
        snapshots = ProviderNormalize.replaceSnapshot(snapshots, {
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
        return ProviderNormalize.replaceSnapshot(snapshots, {
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

    SignalSpy {
        id: sortModeSpy
        target: fullView
        signalName: "sortModeRequested"
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
            fullView.sortMode = "default"
            host.testProviders = DisplayProvider.buildDisplay(
                                    host.tokenHubDefinitions(),
                                    host.liveSnapshots())
            refreshSpy.clear()
            configureSpy.clear()
            keepOpenSpy.clear()
            sortModeSpy.clear()
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

        function test_sort_button_cycles_through_every_mode() {
            const sortButton = findChild(fullView, "sortButton")
            const expectedModes = [
                "alphabetical", "usedPercent", "remainingPercent",
                "nextReset", "custom", "default"
            ]

            verify(sortButton !== null)
            for (let i = 0; i < expectedModes.length; ++i) {
                mouseClick(sortButton)
                compare(sortModeSpy.count, i + 1)
                compare(sortModeSpy.signalArguments[i][0], expectedModes[i])
                fullView.sortMode = expectedModes[i]
            }
        }


        function test_pie_panel_renders_models_and_windows() {
            host.configuredCompactStyle = "pie"
            wait(0)

            const providerScroll = findChild(fullView, "providerScroll")
            const pieScroll = findChild(fullView, "pieScroll")
            verify(providerScroll !== null)
            verify(pieScroll !== null)
            compare(providerScroll.visible, false)
            compare(pieScroll.visible, true)
            compare(descendantsNamed(fullView, "panelPieProvider").length, 3)
            compare(descendantsNamed(fullView, "panelPiePlan").length, 6)
        }

        function test_template_uses_independent_values() {
            const bars = descendantsNamed(fullView, "planBar")
            compare(bars.length, 6)

            const detail = findChild(bars[5], "templateTextLabel")
            verify(detail !== null)
            compare(detail.text, "周限额  503/750  重置于 周日 00:00")
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

        function test_usage_segments_render_in_popup_progress_bar() {
            host.testProviders = [{
                id: "codexzh",
                providerName: "CodexZH",
                statusLabel: "可用",
                errorText: "",
                ledClass: "led-green",
                plans: [{
                    planName: "周限额", usedPercent: 50, usedPercentLabel: "50%",
                    barClass: "bar-green", usedText: "50", totalText: "100",
                    templateText: "%1 限额  %2/%3  重置于 %4", resetText: "周一 00:00",
                    unitText: "USD", extraText: "",
                    usageSegments: [{
                        kind: "previous", used: 20, usedPercent: 20, formattedUsed: "$20.00"
                    }, {
                        kind: "today", used: 30, usedPercent: 30, formattedUsed: "$30.00"
                    }]
                }]
            }]
            wait(0)

            const bar = descendantsNamed(fullView, "planBar")[0]
            const previous = findChild(bar, "usagePreviousSegment")
            const current = findChild(bar, "usageCurrentSegment")
            verify(previous !== null)
            verify(current !== null)
            compare(previous.width, findChild(bar, "planProgressBar").width * 0.2)
            compare(current.width, findChild(bar, "planProgressBar").width * 0.5)
            compare(current.radius, current.height / 2)
            compare(previous.Accessible.name, "此前使用 · 20% · $20.00")
            compare(current.Accessible.name, "今日使用 · 30% · $30.00")
        }

        function test_progress_bar_accepts_cpp_array_like_segments() {
            host.testProviders = [{
                id: "codexzh", providerName: "CodexZH", statusLabel: "可用",
                errorText: "", ledClass: "led-green",
                plans: [{
                    planName: "周限额", usedPercent: 50, usedPercentLabel: "50%",
                    barClass: "bar-green", usedText: "50", totalText: "100",
                    usageSegments: {
                        0: { kind: "previous", used: 20, usedPercent: 20, formattedUsed: "$20.00" },
                        1: { kind: "today", used: 30, usedPercent: 30, formattedUsed: "$30.00" },
                        length: 2
                    }
                }]
            }]
            wait(0)

            const bar = descendantsNamed(fullView, "planBar")[0]
            verify(findChild(bar, "usagePreviousSegment") !== null)
        }

        function test_usage_segments_are_hit_in_popup_pie() {
            host.configuredCompactStyle = "pie"
            host.testProviders = [{
                id: "codexzh", providerName: "CodexZH", statusLabel: "可用",
                errorText: "", ledClass: "led-green",
                plans: [{
                    planName: "周限额", usedPercent: 50, usedPercentLabel: "50%",
                    barClass: "bar-green", usageSegments: [{
                        kind: "previous", used: 20, usedPercent: 20, formattedUsed: "$20.00"
                    }, {
                        kind: "today", used: 30, usedPercent: 30, formattedUsed: "$30.00"
                    }]
                }]
            }]
            wait(50)

            const pie = findChild(fullView, "panelPieChart")
            verify(pie !== null)
            verify(pie.outerRadius > 0)
            verify(pie.hasUsageSegments)
            const radius = pie.outerRadius - pie.ringThickness / 2
            compare(pie.usageSegmentAt(pie.width / 2, pie.height / 2 - radius), 0)
            compare(pie.usageSegmentAt(pie.width / 2 + Math.sin(Math.PI * 0.6) * radius,
                                       pie.height / 2 - Math.cos(Math.PI * 0.6) * radius), 1)
            compare(pie.usageSegmentAt(pie.width / 2, pie.height / 2), -1)
            compare(pie.usageSegmentAt(0, 0), -1)
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

        function test_price_and_total_visible() {
            const defs = host.tokenHubDefinitions()
            defs[0].price = 30                       // minimax
            host.testProviders = DisplayProvider.buildDisplay(
                defs, host.liveSnapshots(), { sortMode: "default" })
            wait(0)

            const priceLabels = descendantsNamed(fullView, "providerPriceLabel")
            const totalLabel = findChild(fullView, "totalPriceLabel")
            verify(priceLabels.length >= 1)
            compare(priceLabels[0].text, "¥30.00")
            verify(totalLabel !== null)
            compare(totalLabel.text, "总价 ¥30.00")
        }

        function test_total_hidden_without_prices() {
            host.testProviders = DisplayProvider.buildDisplay(
                host.tokenHubDefinitions(), host.liveSnapshots(), { sortMode: "default" })
            wait(0)

            const totalLabel = findChild(fullView, "totalPriceLabel")
            verify(totalLabel === null || !totalLabel.visible)
        }
    }
}
