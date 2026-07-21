import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import "../js/mockData.js" as MockData

PlasmoidItem {
    id: root

    property var providerDefinitions: MockData.normalizeDefinitions(Plasmoid.configuration.providers)
    property var runtimeSnapshots: MockData.createSeedSnapshots(providerDefinitions)
    property var providers: MockData.buildDisplayProviders(providerDefinitions, runtimeSnapshots)
    readonly property var tightestUsage: MockData.tightestUsage(providers)
    readonly property string compactStyle: Plasmoid.configuration.compactStyle || "pie"
    readonly property int refreshIntervalSec: Math.max(10,
                                                        Plasmoid.configuration.refreshIntervalSec || 60)
    readonly property int opacityPercent: Math.max(20, Math.min(100,
                                                       Plasmoid.configuration.opacityPercent || 80))
    readonly property bool keepPanelOpen: Boolean(Plasmoid.configuration.keepPanelOpen)

    hideOnWindowDeactivate: !keepPanelOpen

    onProviderDefinitionsChanged: {
        runtimeSnapshots = MockData.createSeedSnapshots(providerDefinitions)
        providers = MockData.buildDisplayProviders(providerDefinitions, runtimeSnapshots)
    }

    function refresh() {
        runtimeSnapshots = MockData.fluctuateSnapshots(runtimeSnapshots)
        providers = MockData.buildDisplayProviders(providerDefinitions, runtimeSnapshots)
    }

    function openConfiguration() {
        const action = Plasmoid.internalAction("configure")
        if (action)
            action.trigger()
    }

    Plasmoid.title: ""
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground

    toolTipMainText: i18n("AI 用量监控")
    toolTipSubText: tightestUsage.usedPercent < 0
        ? i18n("暂无可用数据")
        : i18n("%1 · %2 · 已用 %3",
               MockData.stripProviderSuffix(tightestUsage.providerName),
               tightestUsage.planName,
               tightestUsage.usedPercent + "%")

    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            text: i18n("配置…")
            icon.name: "configure"
            onTriggered: root.openConfiguration()
        },
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

    compactRepresentation: CompactView {
        providers: root.providers
        compactStyle: root.compactStyle
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
