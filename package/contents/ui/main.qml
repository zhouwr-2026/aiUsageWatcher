pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import "../js/mockData.js" as MockData

PlasmoidItem {
    id: root

    property var providerDefinitions: MockData.normalizeDefinitions(Plasmoid.configuration.providers)
    property var runtimeSnapshots: MockData.createSeedSnapshots(providerDefinitions)
    property var providers: MockData.buildDisplayProviders(providerDefinitions, runtimeSnapshots)
    property int compactProviderIndex: 0
    readonly property var compactUsage: MockData.providerUsageAt(providers, compactProviderIndex)
    readonly property string compactStyle: Plasmoid.configuration.compactStyle || "pie"
    readonly property int refreshIntervalSec: Math.max(10,
                                                        Plasmoid.configuration.refreshIntervalSec || 60)
    readonly property int opacityPercent: Math.max(20, Math.min(100,
                                                       Plasmoid.configuration.opacityPercent || 80))
    readonly property bool keepPanelOpen: Boolean(Plasmoid.configuration.keepPanelOpen)
    // Plasma 6 exposes the concrete Plasma::Applet subclass through the
    // attached Plasmoid object. Bracket access below keeps the QML-only test
    // environment compatible when the native plugin is not instantiated.
    readonly property var usageBackend: Plasmoid

    activationTogglesExpanded: true
    hideOnWindowDeactivate: !keepPanelOpen

    onProviderDefinitionsChanged: {
        compactProviderIndex = 0
        runtimeSnapshots = MockData.createSeedSnapshots(providerDefinitions)
        applyMiniMaxSnapshot()
        providers = MockData.buildDisplayProviders(providerDefinitions, runtimeSnapshots)
    }

    function applyMiniMaxSnapshot() {
        const snapshot = usageBackend["miniMaxSnapshot"]
        if (!snapshot || snapshot.providerId !== "minimax"
                || !Array.isArray(snapshot.plans))
            return false

        runtimeSnapshots = MockData.replaceSnapshot(runtimeSnapshots, snapshot)
        providers = MockData.buildDisplayProviders(providerDefinitions, runtimeSnapshots)
        return true
    }

    function requestMiniMaxRefresh() {
        const refreshFunction = usageBackend["refreshMiniMax"]
        if (typeof refreshFunction !== "function")
            return false
        refreshFunction.call(usageBackend)
        return true
    }

    function refresh() {
        runtimeSnapshots = MockData.fluctuateSnapshots(runtimeSnapshots)
        providers = MockData.buildDisplayProviders(providerDefinitions, runtimeSnapshots)
        requestMiniMaxRefresh()
    }

    function openConfiguration() {
        const action = Plasmoid.internalAction("configure")
        if (action)
            action.trigger()
    }

    Plasmoid.title: i18n("AI 用量监控")
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground

    toolTipMainText: i18n("AI 用量监控")
    toolTipSubText: compactUsage.usedPercent < 0
        ? (compactUsage.providerName
           ? i18n("%1 · 暂无可用数据",
                  MockData.stripProviderSuffix(compactUsage.providerName))
           : i18n("暂无可用数据"))
        : i18n("%1 · %2 · 已用 %3",
               MockData.stripProviderSuffix(compactUsage.providerName),
               compactUsage.planName,
               compactUsage.usedPercent + "%")

    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            text: i18n("刷新")
            icon.name: "view-refresh"
            onTriggered: root.refresh()
        }
    ]

    Timer {
        interval: root.refreshIntervalSec * 1000
        running: true
        repeat: true
        onTriggered: root.refresh()
    }

    Timer {
        interval: 5000
        running: root.providers.length > 1
        repeat: true
        onTriggered: root.compactProviderIndex = MockData.nextProviderIndexWithUsage(
                         root.providers, root.compactProviderIndex)
    }

    Connections {
        target: root.usageBackend
        ignoreUnknownSignals: true

        function onMiniMaxSnapshotChanged() {
            root.applyMiniMaxSnapshot()
        }
    }

    Component.onCompleted: {
        applyMiniMaxSnapshot()
        requestMiniMaxRefresh()
    }

    compactRepresentation: CompactView {
        providers: root.providers
        compactStyle: root.compactStyle
        providerIndex: root.compactProviderIndex
        plasmoidItem: root
    }

    fullRepresentation: FullView {
        providers: root.providers
        opacityPercent: root.opacityPercent
        keepPanelOpen: root.keepPanelOpen
        onRefreshRequested: root.refresh()
        onConfigureRequested: root.openConfiguration()
        onKeepOpenChanged: keepOpen => Plasmoid.configuration.keepPanelOpen = keepOpen
    }
}
