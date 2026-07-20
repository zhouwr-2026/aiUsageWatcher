import QtQuick
import QtQuick.Layouts
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.plasmoid

PlasmoidItem {
    id: root

    // 桌面/面板两种状态都允许：根据容器大小自动切换
    // 小尺寸（面板、桌面图标）显示 compact，大尺寸直接展开 full

    // 桌面图标的 compact view：圆球显示最紧张的"已用 %"
    Plasmoid.compactRepresentation: MouseArea {
        id: compactRoot
        Layout.minimumWidth: PlasmaCore.Units.gridUnit * 2
        Layout.minimumHeight: PlasmaCore.Units.gridUnit * 2
        Layout.preferredWidth: PlasmaCore.Units.gridUnit * 4
        Layout.preferredHeight: PlasmaCore.Units.gridUnit * 4
        onClicked: plasmoid.expanded = !plasmoid.expanded

        Orb {
            anchors.fill: parent
            // TODO(usage): 绑定数据源后改成真实 %，颜色同 orb-* 三态
            usedPercent: 0
            usedPercentLabel: "—"
            ringClass: "orb-gray"
            providerName: ""
        }
    }

    // 弹出/全屏 view：显示供应商用量 + 重置时间 + 右上角配置/固定按钮
    Plasmoid.fullRepresentation: Item {
        Layout.minimumWidth: PlasmaCore.Units.gridUnit * 22
        Layout.minimumHeight: PlasmaCore.Units.gridUnit * 18
        Layout.preferredWidth: PlasmaCore.Units.gridUnit * 30
        Layout.preferredHeight: PlasmaCore.Units.gridUnit * 24

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: PlasmaCore.Units.smallSpacing

            // 顶部工具栏：右侧两个按钮，仿 KDE 顶部时间小部件
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                ToolButton { text: i18n("配置") }
                ToolButton { text: i18n("固定") }
            }

            // 圆球 + 用量详情
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Orb {
                    Layout.preferredWidth: PlasmaCore.Units.gridUnit * 5
                    Layout.preferredHeight: PlasmaCore.Units.gridUnit * 5
                    usedPercent: 0
                    usedPercentLabel: "—"
                    ringClass: "orb-gray"
                    providerName: ""
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Label { text: "暂无数据" }
                }
            }
        }
    }
}