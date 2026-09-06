pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import "../js/providerNormalize.js" as ProviderNormalize
import "../js/displayProvider.js" as DisplayProvider

PlasmoidItem {
    id: root

    readonly property var usageBackend: Plasmoid
    readonly property string sharedProvidersRaw:
        typeof usageBackend["sharedProviders"] === "string"
        ? usageBackend["sharedProviders"] : ""
    property var providerDefinitions: ProviderNormalize.normalizeDefinitions(
        sharedProvidersRaw.length > 0
        ? sharedProvidersRaw : Plasmoid.configuration.providers)
    property var runtimeSnapshots: []
    readonly property string effectiveSortMode: Plasmoid.configuration.sortMode || "default"
    readonly property string customOrderRaw: Plasmoid.configuration.customOrder || ""
    readonly property var providers: DisplayProvider.buildDisplay(
        providerDefinitions, runtimeSnapshots, {
            sortMode: root.effectiveSortMode,
            customOrderRaw: root.customOrderRaw
        })
    Component.onCompleted: {
        const ensureShared = usageBackend["ensureSharedProviders"]
        if (sharedProvidersRaw.length === 0 && typeof ensureShared === "function") {
            const localProviders = typeof Plasmoid.configuration.providers === "string"
                && Plasmoid.configuration.providers.length > 0
                ? Plasmoid.configuration.providers : JSON.stringify(providerDefinitions)
            ensureShared.call(usageBackend, localProviders)
        }
        console.log("[LOGO] count=", providers.length)
        for (var i = 0; i < providers.length; ++i) {
            console.log("[LOGO]", i, providers[i].providerName, "logoSource=", providers[i].logoSource, "logoIsSvg=", providers[i].logoIsSvg)
        }
        applyMiniMaxSnapshot()
        applyDeepSeekSnapshot()
        applyCodexSnapshot()
        applyCodexZhSnapshot()
        applyOpenCodeGoSnapshot()
        applyAgnesSnapshot()
        applyCommandCodeSnapshot()
        applyCustomSnapshots()
        requestMiniMaxRefresh()
        requestDeepSeekRefresh()
        requestCodexRefresh()
        requestCodexZhRefresh()
        requestOpenCodeGoRefresh()
        requestAgnesRefresh()
        requestCommandCodeRefresh()
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
    activationTogglesExpanded: true
    hideOnWindowDeactivate: !keepPanelOpen

    onProviderDefinitionsChanged: {
        compactProviderIndex = 0
        runtimeSnapshots = ProviderNormalize.createSeedSnapshots(providerDefinitions)
        applyMiniMaxSnapshot()
        applyDeepSeekSnapshot()
        applyCodexSnapshot()
        applyCodexZhSnapshot()
        applyCustomSnapshots()
        requestCustomRefresh()
    }

    // 通用快照接入：按 backend 属性名与 providerId 匹配，命中则替换运行时快照。
    // 示例：applySnapshotFor("miniMaxSnapshot", "minimax")
    function applySnapshotFor(backendKey, providerId) {
        const snapshot = usageBackend[backendKey]
        if (!snapshot || snapshot.providerId !== providerId
                || !snapshot.plans || typeof snapshot.plans.length !== "number")
            return false

        runtimeSnapshots = ProviderNormalize.replaceSnapshot(runtimeSnapshots, snapshot)
        lastRefreshTime = new Date()
        return true
    }

    // 通用刷新入口：调用 applet 的 Q_INVOKABLE；可选透传参数（如 custom 的 definitions）。
    function requestRefreshFor(backendMethod, arg) {
        const refreshFunction = usageBackend[backendMethod]
        if (typeof refreshFunction !== "function")
            return false
        if (arguments.length > 1)
            refreshFunction.call(usageBackend, arg)
        else
            refreshFunction.call(usageBackend)
        return true
    }

    function applyMiniMaxSnapshot() {
        return applySnapshotFor("miniMaxSnapshot", "minimax")
    }

    function requestMiniMaxRefresh() {
        return requestRefreshFor("refreshMiniMax")
    }

    function applyDeepSeekSnapshot() {
        return applySnapshotFor("deepseekSnapshot", "deepseek")
    }

    function requestDeepSeekRefresh() {
        return requestRefreshFor("refreshDeepSeekUsage")
    }

    function applyCodexSnapshot() {
        return applySnapshotFor("codexSnapshot", "codex")
    }

    function requestCodexRefresh() {
        return requestRefreshFor("refreshCodexUsage")
    }

    function applyCodexZhSnapshot() {
        return applySnapshotFor("codexzhSnapshot", "codexzh")
    }

    function requestCodexZhRefresh() {
        return requestRefreshFor("refreshCodexZhUsage")
    }

    function applyOpenCodeGoSnapshot() {
        return applySnapshotFor("opencodeGoSnapshot", "opencode-go")
    }

    function requestOpenCodeGoRefresh() {
        return requestRefreshFor("refreshOpenCodeGoUsage")
    }

    function applyAgnesSnapshot() {
        return applySnapshotFor("agnesSnapshot", "agnes-ai")
    }

    function requestAgnesRefresh() {
        return requestRefreshFor("refreshAgnesUsage")
    }

    function applyCommandCodeSnapshot() {
        return applySnapshotFor("commandCodeSnapshot", "command-code")
    }

    function requestCommandCodeRefresh() {
        return requestRefreshFor("refreshCommandCodeUsage")
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
        // custom 必须透传 providerDefinitions（脚本型 provider 由 QML 侧提供定义）。
        return requestRefreshFor("refreshCustomProviders", providerDefinitions)
    }

    function refresh() {
        runtimeSnapshots = ProviderNormalize.createSeedSnapshots(providerDefinitions)
        applyMiniMaxSnapshot()
        applyDeepSeekSnapshot()
        applyCodexSnapshot()
        applyCodexZhSnapshot()
        applyOpenCodeGoSnapshot()
        applyAgnesSnapshot()
        applyCommandCodeSnapshot()
        applyCustomSnapshots()
        lastRefreshTime = new Date()
        requestMiniMaxRefresh()
        requestDeepSeekRefresh()
        requestCodexRefresh()
        requestCodexZhRefresh()
        requestOpenCodeGoRefresh()
        requestAgnesRefresh()
        requestCommandCodeRefresh()
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
        : qsTr("%1 · %2 · 已用 %3%4")
            .arg(ProviderNormalize.stripProviderSuffix(compactUsage.providerName))
            .arg(compactUsage.planName)
            .arg(compactUsage.usedPercent + "%")
            .arg(compactUsage.stale ? qsTr("（暂未更新）") : "")

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
        function onDeepseekSnapshotChanged() {
            root.applyDeepSeekSnapshot()
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
        function onOpencodeGoSnapshotChanged() {
            root.applyOpenCodeGoSnapshot()
        }
        function onAgnesSnapshotChanged() {
            root.applyAgnesSnapshot()
        }
        function onCommandCodeSnapshotChanged() {
            root.applyCommandCodeSnapshot()
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

    function setFullPanelHeight(height) {
        // 持久化到 Plasma 记忆的 popup 尺寸键：下次打开面板沿用
        Plasmoid.configuration.popupHeight = Math.round(height)
        // 直接修改 popup 窗口高度（不重开面板）：fullRepresentationItem 的
        // Window 即 Plasma 的 popup 窗口，改其 height 立即生效
        const full = fullRepresentationItem
        if (full) {
            full.implicitHeight = height
            full.Layout.preferredHeight = height
            const popupWindow = full.Window.window
            if (popupWindow)
                popupWindow.height = height
        }
    }

    fullRepresentation: FullView {
        providers: root.providers
        opacityPercent: root.opacityPercent
        keepPanelOpen: root.keepPanelOpen
        panelStyle: root.panelStyle
        lastRefreshTime: root.lastRefreshTime
        sortMode: root.effectiveSortMode
        onSortModeRequested: mode => Plasmoid.configuration.sortMode = mode
        onRefreshRequested: root.refresh()
        onCloseRequested: root.expanded = false
        onConfigureRequested: root.openConfiguration()
        onHeightRequested: height => root.setFullPanelHeight(height)
        onKeepOpenChanged: keepOpen => Plasmoid.configuration.keepPanelOpen = keepOpen
    }
}
