import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import "../js/mockData.js" as MockData

Item {
    id: fullRoot

    property var providers: []
    property string groupBy: "provider"  // "provider" | "window"
    signal refreshRequested()

    Layout.minimumWidth: PlasmaCore.Units.gridUnit * 32
    Layout.minimumHeight: PlasmaCore.Units.gridUnit * 4
    Layout.preferredWidth: PlasmaCore.Units.gridUnit * 36
    Layout.preferredHeight: PlasmaCore.Units.gridUnit * 24

    // 面板背景
    Rectangle {
        anchors.fill: parent
        radius: PlasmaCore.Units.smallSpacing
        color: Qt.rgba(PlasmaCore.Theme.backgroundColor.r,
                       PlasmaCore.Theme.backgroundColor.g,
                       PlasmaCore.Theme.backgroundColor.b, 0.92)
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
                color: PlasmaCore.Theme.TextColor
                font.bold: true
                font.pixelSize: PlasmaCore.Units.gridUnit * 0.85
                Layout.fillWidth: true
            }

            // 刷新按钮(点击转圈 + emit refreshRequested)
            PlasmaComponents.ToolButton {
                id: refreshBtn
                icon.name: "view-refresh"
                checkable: true
                onClicked: {
                    checked = true;
                    fullRoot.refreshRequested();
                    refreshTimer.restart();
                }
                Timer {
                    id: refreshTimer
                    interval: 300
                    onTriggered: refreshBtn.checked = false
                }
            }

            PlasmaComponents.ToolButton {
                icon.name: "configure"
                onClicked: plasmoid.action("configure").trigger()
            }

            PlasmaComponents.ToolButton {
                icon.name: "window-pin"
                checkable: true
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
            model: fullRoot.providers
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
                                case "led-green": return PlasmaCore.Theme.PositiveText;
                                case "led-yellow": return PlasmaCore.Theme.HighlightColor;
                                case "led-red": return PlasmaCore.Theme.NegativeText;
                                default: return PlasmaCore.Theme.NeutralText;
                                }
                            }
                        }
                        PlasmaComponents.Label {
                            text: MockData.stripProviderSuffix(modelData.providerName)
                            color: PlasmaCore.Theme.TextColor
                            font.bold: true
                            font.pixelSize: PlasmaCore.Units.gridUnit * 0.7
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        PlasmaComponents.Label {
                            visible: modelData.sourceLabel.length > 0
                            text: modelData.sourceLabel
                            color: PlasmaCore.Theme.TextColor
                            opacity: 0.7
                            font.pixelSize: PlasmaCore.Units.gridUnit * 0.5
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        PlasmaComponents.Label {
                            visible: modelData.statusLabel.length > 0
                            text: modelData.statusLabel
                            color: {
                                switch (modelData.ledClass) {
                                case "led-green": return PlasmaCore.Theme.PositiveText;
                                case "led-yellow": return PlasmaCore.Theme.HighlightColor;
                                case "led-red": return PlasmaCore.Theme.NegativeText;
                                default: return PlasmaCore.Theme.NeutralText;
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
                        color: PlasmaCore.Theme.NegativeText
                        width: parent.width
                        wrapMode: Text.Wrap
                        font.pixelSize: PlasmaCore.Units.gridUnit * 0.55
                    }

                    // PlanBar 区域(沿用现状布局)
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

                                    // 进度条(用户硬约束:不改布局)
                                    Item {
                                        width: parent.width - 40
                                        height: 6
                                        anchors.verticalCenter: parent.verticalCenter

                                        Rectangle {
                                            anchors.fill: parent; radius: 3
                                            color: Qt.rgba(PlasmaCore.Theme.backgroundColor.r,
                                                           PlasmaCore.Theme.backgroundColor.g,
                                                           PlasmaCore.Theme.backgroundColor.b, 0.4)
                                        }
                                        Rectangle {
                                            anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                            width: parent.width * (modelData.usedPercent / 100)
                                            radius: 3
                                            color: {
                                                switch (modelData.barClass) {
                                                case "bar-green": return PlasmaCore.Theme.PositiveText;
                                                case "bar-yellow": return PlasmaCore.Theme.HighlightColor;
                                                case "bar-red": return PlasmaCore.Theme.NegativeText;
                                                default: return PlasmaCore.Theme.NeutralText;
                                                }
                                            }
                                            Behavior on width {
                                                NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                                            }
                                        }
                                    }

                                    // 模板渲染:计划名 + 百分比 + 重置时间(i18np 合并)
                                    PlasmaComponents.Label {
                                        Layout.fillWidth: true
                                        text: i18np(
                                            modelData.template || "%1 限额  %2/%3  重置于 %4",
                                            modelData.planName,
                                            modelData.usedText || modelData.usedPercentLabel,
                                            "",
                                            modelData.resetText
                                        )
                                        color: PlasmaCore.Theme.TextColor
                                        opacity: 0.85
                                        elide: Text.ElideRight
                                        font.pixelSize: PlasmaCore.Units.gridUnit * 0.55
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
