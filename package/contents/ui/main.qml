import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import "../js/mockData.js" as MockData

PlasmoidItem {
    id: root

    property var providers: MockData.SEED_PROVIDERS

    Plasmoid.title: i18n("AI 用量监控")
    toolTipMainText: i18n("AI 用量监控")
    toolTipSubText: {
        if (providers.length === 0) return i18n("暂无供应商数据");
        let lines = [];
        for (const p of providers) {
            let name = MockData.stripProviderSuffix(p.providerName);
            let worst = -1;
            for (const plan of p.plans) {
                if (typeof plan.usedPercent === "number" && plan.usedPercent > worst)
                    worst = plan.usedPercent;
            }
            if (worst >= 0) {
                lines.push(name + ": " + Math.round(worst) + "%");
            } else if (p.errorText) {
                lines.push(name + ": " + i18n("异常"));
            }
        }
        return lines.join("  ·  ");
    }

    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            text: i18n("配置…")
            icon.name: "configure"
            onTriggered: plasmoid.action("configure").trigger()
        },
        PlasmaCore.Action {
            text: i18n("刷新")
            icon.name: "view-refresh"
            onTriggered: root.providers = MockData.fluctuateProviders(root.providers)
        }
    ]

    function tightestUsedPercent() {
        let worst = -1;
        for (const p of providers) {
            for (const plan of p.plans) {
                if (typeof plan.usedPercent === "number" && plan.usedPercent > worst)
                    worst = plan.usedPercent;
            }
        }
        return worst;
    }

    Timer {
        interval: 60000
        running: true
        repeat: true
        onTriggered: root.providers = MockData.fluctuateProviders(root.providers)
    }

    // ── Compact：圆球 ──────────────────────────────────
    compactRepresentation: MouseArea {
        id: compactRoot

        Layout.minimumWidth: PlasmaCore.Units.gridUnit * 3
        Layout.minimumHeight: PlasmaCore.Units.gridUnit * 3
        Layout.preferredWidth: PlasmaCore.Units.gridUnit * 4
        Layout.preferredHeight: PlasmaCore.Units.gridUnit * 4

        hoverEnabled: true
        onClicked: plasmoid.expanded = !plasmoid.expanded

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: Qt.rgba(0.04, 0.05, 0.1, 0.85)
            border.width: 2
            border.color: {
                const p = root.tightestUsedPercent();
                if (p < 0) return Qt.rgba(1, 1, 1, 0.12);
                if (p <= 5) return Qt.rgba(0.97, 0.44, 0.44, 0.75);
                if (p <= 15) return Qt.rgba(0.98, 0.75, 0.14, 0.65);
                return Qt.rgba(0.2, 0.82, 0.6, 0.65);
            }
        }

        Text {
            anchors.centerIn: parent
            text: root.tightestUsedPercent() >= 0 ? Math.round(root.tightestUsedPercent()) + "%" : "—"
            color: {
                const p = root.tightestUsedPercent();
                if (p < 0) return "#9ca3af";
                if (p <= 5) return "#f87171";
                if (p <= 15) return "#fbbf24";
                return "#34d399";
            }
            font.pixelSize: PlasmaCore.Units.gridUnit * 1.2
            font.bold: true
        }
    }

    // ── Full：弹出面板 ──────────────────────────────────
    fullRepresentation: Item {
        id: fullRoot

        Layout.minimumWidth: PlasmaCore.Units.gridUnit * 32
        Layout.minimumHeight: PlasmaCore.Units.gridUnit * 4
        Layout.preferredWidth: PlasmaCore.Units.gridUnit * 36
        Layout.preferredHeight: PlasmaCore.Units.gridUnit * 24

        // 面板背景
        Rectangle {
            anchors.fill: parent
            radius: PlasmaCore.Units.smallSpacing
            color: Qt.rgba(0.06, 0.08, 0.14, 0.92)
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: PlasmaCore.Units.gridUnit * 0.5
            spacing: PlasmaCore.Units.smallSpacing

            // 标题栏
            RowLayout {
                Layout.fillWidth: true
                spacing: PlasmaCore.Units.smallSpacing

                PlasmaComponents.Label {
                    text: i18n("AI 用量监控")
                    color: "#f1f5f9"
                    font.bold: true
                    font.pixelSize: PlasmaCore.Units.gridUnit * 0.85
                    Layout.fillWidth: true
                }

                PlasmaComponents.ToolButton {
                    icon.name: "configure"
                    onClicked: plasmoid.action("configure").trigger()
                }

                PlasmaComponents.ToolButton {
                    icon.name: "window-pin"
                    checkable: true
                    checked: false
                    onCheckedChanged: plasmoid.expanded = false
                }
            }

            // 分隔线
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(1, 1, 1, 0.06)
            }

            // 供应商列表
            ListView {
                id: listView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.providers
                spacing: 4

                delegate: Item {
                    required property var modelData
                    property int idx: index
                    width: ListView.view.width
                    height: column.implicitHeight + 4

                    Column {
                        id: column
                        width: parent.width
                        spacing: 2
                        topPadding: 4
                        leftPadding: 4
                        rightPadding: 4

                        // 供应商标题
                        Row {
                            width: parent.width
                            spacing: 4
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                anchors.verticalCenter: parent.verticalCenter
                                color: {
                                    switch (modelData.ledClass) {
                                    case "led-green": return "#34d399";
                                    case "led-yellow": return "#fbbf24";
                                    case "led-red": return "#f87171";
                                    default: return "#6b7280";
                                    }
                                }
                            }
                            PlasmaComponents.Label {
                                text: MockData.stripProviderSuffix(modelData.providerName)
                                color: "#e2e8f0"
                                font.bold: true
                                font.pixelSize: PlasmaCore.Units.gridUnit * 0.7
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            PlasmaComponents.Label {
                                visible: modelData.sourceLabel.length > 0
                                text: modelData.sourceLabel
                                color: "#94a3b8"
                                font.pixelSize: PlasmaCore.Units.gridUnit * 0.5
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            PlasmaComponents.Label {
                                visible: modelData.statusLabel.length > 0
                                text: modelData.statusLabel
                                color: {
                                    switch (modelData.ledClass) {
                                    case "led-green": return "#34d399";
                                    case "led-yellow": return "#fbbf24";
                                    case "led-red": return "#f87171";
                                    default: return "#9ca3af";
                                    }
                                }
                                font.pixelSize: PlasmaCore.Units.gridUnit * 0.5
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        // 错误信息
                        PlasmaComponents.Label {
                            visible: modelData.errorText.length > 0
                            text: modelData.errorText
                            color: "#fca5a5"
                            width: parent.width
                            wrapMode: Text.Wrap
                            font.pixelSize: PlasmaCore.Units.gridUnit * 0.55
                        }

                        // PlanBar
                        Column {
                            width: parent.width
                            spacing: 1
                            visible: modelData.plans.length > 0

                            Repeater {
                                model: modelData.plans

                                delegate: Item {
                                    required property var modelData
                                    width: parent.width
                                    height: planRow.implicitHeight + 2

                                    Row {
                                        id: planRow
                                        width: parent.width
                                        spacing: 4
                                        leftPadding: 8

                                        PlasmaComponents.Label {
                                            text: modelData.planName
                                            color: "#94a3b8"
                                            width: 60
                                            elide: Text.ElideRight
                                            font.pixelSize: PlasmaCore.Units.gridUnit * 0.55
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        Item {
                                            width: parent.width - 220
                                            height: 6
                                            anchors.verticalCenter: parent.verticalCenter

                                            Rectangle {
                                                anchors.fill: parent; radius: 3
                                                color: Qt.rgba(1, 1, 1, 0.08)
                                            }
                                            Rectangle {
                                                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                                width: parent.width * (modelData.usedPercent / 100)
                                                radius: 3
                                                color: {
                                                    switch (modelData.barClass) {
                                                    case "bar-green": return "#34d399";
                                                    case "bar-yellow": return "#fbbf24";
                                                    case "bar-red": return "#f87171";
                                                    default: return "#6b7280";
                                                    }
                                                }
                                                Behavior on width {
                                                    NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                                                }
                                            }
                                        }

                                        PlasmaComponents.Label {
                                            text: modelData.usedPercentLabel
                                            color: {
                                                switch (modelData.barClass) {
                                                case "bar-green": return "#34d399";
                                                case "bar-yellow": return "#fbbf24";
                                                case "bar-red": return "#f87171";
                                                default: return "#9ca3af";
                                                }
                                            }
                                            font.bold: true
                                            font.pixelSize: PlasmaCore.Units.gridUnit * 0.55
                                            width: 40
                                            horizontalAlignment: Text.AlignRight
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        PlasmaComponents.Label {
                                            text: modelData.resetText
                                            color: "#64748b"
                                            font.pixelSize: PlasmaCore.Units.gridUnit * 0.5
                                            visible: modelData.resetText.length > 0
                                            width: 80
                                            elide: Text.ElideRight
                                            horizontalAlignment: Text.AlignRight
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}