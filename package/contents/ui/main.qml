pragma ComponentBehavior: Bound

import QtQuick
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import "../js/providerNormalize.js" as ProviderNormalize
import "../js/displayProvider.js" as DisplayProvider

PlasmoidItem {
    id: root

    property var providerDefinitions: ProviderNormalize.normalizeDefinitions(Plasmoid.configuration.providers)
    property var runtimeSnapshots: []
    readonly property string effectiveSortMode: Plasmoid.configuration.sortMode || "default"
    readonly property string customOrderRaw: Plasmoid.configuration.customOrder || ""
    readonly property var providers: DisplayProvider.buildDisplay(
        providerDefinitions, runtimeSnapshots, {
            sortMode: root.effectiveSortMode,
            customOrderRaw: root.customOrderRaw
        })
    Component.onCompleted: {
        console.log("[LOGO] count=", providers.length)
        for (var i = 0; i < providers.length; ++i) {
            console.log("[LOGO]", i, providers[i].providerName, "logoSource=", providers[i].logoSource, "logoIsSvg=", providers[i].logoIsSvg)
        }
        applyMiniMaxSnapshot()
        applyCodexSnapshot()
        applyCodexZhSnapshot()
        applyCustomSnapshots()
        requestMiniMaxRefresh()
        requestCodexRefresh()
        requestCodexZhRefresh()
        requestCustomRefresh()
    }
    property int compactProviderIndex: 0
    property int restoreProviderIndex: 0
    property bool eventHighlighted: false
    property date lastRefreshTime: new Date()
    readonly property var compactUsage: ProviderNormalize.providerUsageAt(providers, compactProviderIndex)
    readonly property string compactStyle: Plasmoid.configuration.compactStyle || "bar"
    readonly property string panelStyle: Plasmoid.configuration.panelStyle || "bar"
    readonly property string displayStrategy: Plasmoid.configuration.displayStrategy || "polling"
    readonly property string eventMode: Plasmoid.configuration.eventMode || "dbus"
    readonly property int pollingIntervalSec: Math.max(1,
                                                        Plasmoid.configuration.pollingIntervalSec || 5)
    readonly property int highlightDurationSec: Math.max(1,
                                                          Plasmoid.configuration.highlightDurationSec || 30)
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
        runtimeSnapshots = ProviderNormalize.createSeedSnapshots(providerDefinitions)
        applyMiniMaxSnapshot()
        applyCodexSnapshot()
        applyCodexZhSnapshot()
        applyCustomSnapshots()
        requestCustomRefresh()
    }

    function applyMiniMaxSnapshot() {
        const snapshot = usageBackend["miniMaxSnapshot"]
        if (!snapshot || snapshot.providerId !== "minimax"
                || !snapshot.plans || typeof snapshot.plans.length !== "number")
            return false

        runtimeSnapshots = ProviderNormalize.replaceSnapshot(runtimeSnapshots, snapshot)
        lastRefreshTime = new Date()
        return true
    }

    function requestMiniMaxRefresh() {
        const refreshFunction = usageBackend["refreshMiniMax"]
        if (typeof refreshFunction !== "function")
            return false
        refreshFunction.call(usageBackend)
        return true
    }

    function applyCodexSnapshot() {
        const snapshot = usageBackend["codexSnapshot"]
        if (!snapshot || snapshot.providerId !== "codex"
                || !snapshot.plans || typeof snapshot.plans.length !== "number")
            return false

        runtimeSnapshots = ProviderNormalize.replaceSnapshot(runtimeSnapshots, snapshot)
        lastRefreshTime = new Date()
        return true
    }

    function requestCodexRefresh() {
        const refreshFunction = usageBackend["refreshCodexUsage"]
        if (typeof refreshFunction !== "function")
            return false
        refreshFunction.call(usageBackend)
        return true
    }

    function applyCodexZhSnapshot() {
        const snapshot = usageBackend["codexzhSnapshot"]
        if (!snapshot || snapshot.providerId !== "codexzh"
                || !snapshot.plans || typeof snapshot.plans.length !== "number")
            return false

        runtimeSnapshots = ProviderNormalize.replaceSnapshot(runtimeSnapshots, snapshot)
        lastRefreshTime = new Date()
        return true
    }

    function requestCodexZhRefresh() {
        const refreshFunction = usageBackend["refreshCodexZhUsage"]
        if (typeof refreshFunction !== "function")
            return false
        refreshFunction.call(usageBackend)
        return true
    }

    function applyCustomSnapshots() {
        const snapshots = usageBackend["customUsageSnapshots"]
        if (!snapshots || typeof snapshots.length !== "number")
            return false

        let nextSnapshots = runtimeSnapshots
        for (let i = 0; i < snapshots.length; ++i)
            nextSnapshots = ProviderNormalize.replaceSnapshot(nextSnapshots, snapshots[i])
        runtimeSnapshots = nextSnapshots
        lastRefreshTime = new Date()
        return true
    }

    function requestCustomRefresh() {
        const refreshFunction = usageBackend["refreshCustomProviders"]
        if (typeof refreshFunction !== "function")
            return false
        refreshFunction.call(usageBackend, providerDefinitions)
        return true
    }

    function refresh() {
        runtimeSnapshots = ProviderNormalize.createSeedSnapshots(providerDefinitions)
        applyMiniMaxSnapshot()
        applyCodexSnapshot()
        applyCodexZhSnapshot()
        applyCustomSnapshots()
        lastRefreshTime = new Date()
        requestMiniMaxRefresh()
        requestCodexRefresh()
        requestCodexZhRefresh()
        requestCustomRefresh()
        refreshTimer.restart()
    }

    function activateModel(modelName) {
        if (typeof modelName !== "string" || modelName.trim().length === 0)
            return false
        const normalizedName = modelName.trim()
        for (let i = 0; i < providers.length; ++i) {
            const provider = providers[i]
            if (provider.id === normalizedName || provider.providerName === normalizedName
                    || ProviderNormalize.stripProviderSuffix(provider.providerName) === normalizedName) {
                if (!eventHighlighted)
                    restoreProviderIndex = compactProviderIndex
                compactProviderIndex = i
                eventHighlighted = true
                highlightTimer.restart()
                return true
            }
        }
        console.warn("QuotaPilot: ignored unknown model event:", normalizedName)
        return false
    }

    function openConfiguration() {
        const action = Plasmoid.internalAction("configure")
        if (action)
            action.trigger()
    }

    Plasmoid.title: qsTr("额度领航员")
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground

    toolTipMainText: qsTr("额度领航员")
    toolTipSubText: compactUsage.usedPercent < 0
        ? (compactUsage.providerName
           ? qsTr("%1 · 暂无可用数据").arg(
                  ProviderNormalize.stripProviderSuffix(compactUsage.providerName))
           : qsTr("暂无可用数据"))
        : qsTr("%1 · %2 · 已用 %3")
            .arg(ProviderNormalize.stripProviderSuffix(compactUsage.providerName))
            .arg(compactUsage.planName)
            .arg(compactUsage.usedPercent + "%")

    toolTipItem: QuotaTooltip {
        provider: root.compactUsage
    }

    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            text: qsTr("刷新")
            icon.name: "view-refresh"
            onTriggered: root.refresh()
        }
    ]

    Timer {
        id: refreshTimer
        interval: root.refreshIntervalSec * 1000
        running: true
        repeat: true
        onTriggered: root.refresh()
    }

    Timer {
        interval: root.pollingIntervalSec * 1000
        running: root.providers.length > 1 && root.displayStrategy === "polling"
        repeat: true
        onTriggered: root.compactProviderIndex = ProviderNormalize.nextProviderIndexWithUsage(
                         root.providers, root.compactProviderIndex)
    }

    Timer {
        id: highlightTimer

        interval: root.highlightDurationSec * 1000
        repeat: false
        onTriggered: {
            root.eventHighlighted = false
            if (root.providers.length > 0)
                root.compactProviderIndex = Math.min(root.restoreProviderIndex,
                                                     root.providers.length - 1)
        }
    }

    Connections {
        target: root.usageBackend
        ignoreUnknownSignals: true

        function onMiniMaxSnapshotChanged() {
            root.applyMiniMaxSnapshot()
        }
        function onCodexSnapshotChanged() {
            root.applyCodexSnapshot()
        }
        function onCustomUsageSnapshotsChanged() {
            root.applyCustomSnapshots()
        }
        function onCodexzhSnapshotChanged() {
            root.applyCodexZhSnapshot()
        }
        function onModelActivated(modelName) {
            if (root.displayStrategy === "event" && root.eventMode === "dbus")
                root.activateModel(modelName)
        }
    }

    compactRepresentation: CompactView {
        providers: root.providers
        compactStyle: root.compactStyle
        providerIndex: root.compactProviderIndex
        highlighted: root.eventHighlighted
        plasmoidItem: root
    }

    fullRepresentation: FullView {
        providers: root.providers
        opacityPercent: root.opacityPercent
        keepPanelOpen: root.keepPanelOpen
        panelStyle: root.panelStyle
        lastRefreshTime: root.lastRefreshTime
        sortMode: root.effectiveSortMode
        onSortModeChanged: mode => {
            Plasmoid.configuration.sortMode = mode
            Qt.callLater(root.refresh)  // 即时拉取 + restart timer
        }
        onRefreshRequested: root.refresh()
        onCloseRequested: root.expanded = false
        onConfigureRequested: root.openConfiguration()
        onKeepOpenChanged: keepOpen => Plasmoid.configuration.keepPanelOpen = keepOpen
    }
}
