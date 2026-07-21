import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.kirigami as Kirigami
import "../js/mockData.js" as MockData

PlasmoidItem {
    id: root

    // ── 状态 ─────────────────────────────────────────
    property var providers: {
        const persisted = plasmoid.configuration.providers
        if (persisted && persisted.length > 0) {
            try {
                return JSON.parse(persisted)
            } catch (e) {
                console.warn("aiUsageWatcher: failed to parse persisted providers, falling back to SEED")
            }
        }
        return MockData.SEED_PROVIDERS
    }
    readonly property string compactStyle: plasmoid.configuration.compactStyle || "pie"
    readonly property string groupBy: plasmoid.configuration.groupBy || "provider"
    readonly property int refreshIntervalSec: plasmoid.configuration.refreshIntervalSec || 60

    // ── 持久化:把 providers 数组写回 KConfig(任何修改后调用) ─────────
    function persistProviders() {
        plasmoid.configuration.providers = JSON.stringify(providers)
    }

    // ── 刷新 ─────────────────────────────────────────
    function refresh() {
        providers = MockData.fluctuateProviders(providers)
        persistProviders()
    }

    Plasmoid.title: ""
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground

    // ── 工具提示 ─────────────────────────────────────────
    toolTipMainText: i18n("AI 用量监控")
    toolTipSubText: i18n("点击查看详情")

    // ── 右键菜单:2 项(用户硬约束:只保留 配置 + 刷新) ─────────
    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            text: i18n("配置…")
            icon.name: "configure"
            onTriggered: plasmoid.action("configure").trigger()
        },
        PlasmaCore.Action {
            text: i18n("刷新")
            icon.name: "view-refresh"
            onTriggered: root.refresh()
        }
    ]

    // ── Timer(刷新间隔可配置) ─────────────────────────────────────────
    Timer {
        interval: root.refreshIntervalSec * 1000
        running: true
        repeat: true
        onTriggered: root.refresh()
    }

    // ── compactRepresentation:小工具图标 ─────────────────────────────────────────
    compactRepresentation: CompactView {
        providers: root.providers
        compactStyle: root.compactStyle
        onToggled: plasmoid.expanded = !plasmoid.expanded
    }

    // ── fullRepresentation:悬浮面板 ─────────────────────────────────────────
    fullRepresentation: FullView {
        providers: root.providers
        groupBy: root.groupBy
        compactStyle: root.compactStyle
        onRefreshRequested: root.refresh()
    }
}