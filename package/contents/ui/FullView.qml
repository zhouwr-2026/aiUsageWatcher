import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import org.kde.kirigami as Kirigami
import "../js/mockData.js" as MockData

Item {
    id: fullRoot

    property var providers: []
    property string groupBy: "provider"  // "provider" | "window"
    /** compactStyle 用于控制每个供应商内部 plan 的排列方式:
     *  "pie" — 水平排布,每个 plan 一张迷你饼图
     *  "bar" — 垂直堆叠,每个 plan 一条进度条 + 模板文本(默认)
     */
    property string compactStyle: "bar"
    signal refreshRequested()

    Layout.minimumWidth: PlasmaCore.Units.gridUnit * 32
    Layout.minimumHeight: PlasmaCore.Units.gridUnit * 4
    Layout.preferredWidth: PlasmaCore.Units.gridUnit * 36
    Layout.preferredHeight: PlasmaCore.Units.gridUnit * 24

    // 面板背景
    Rectangle {
        anchors.fill: parent
        radius: PlasmaCore.Units.smallSpacing
        color: Qt.rgba(Kirigami.Theme.backgroundColor.r,
                       Kirigami.Theme.backgroundColor.g,
                       Kirigami.Theme.backgroundColor.b, 0.92)
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
                color: Kirigami.Theme.textColor
                font.bold: true
                font.pixelSize: PlasmaCore.Units.gridUnit * 0.85
                Layout.fillWidth: true
            }

            // 刷新按钮
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

                    // 供应商标题行
                    Row {
                        width: parent.width
                        spacing: 4
                        Rectangle {
                            width: 6; height: 6; radius: 3
                            anchors.verticalCenter: parent.verticalCenter
                            color: {
                                switch (modelData.ledClass) {
                                case "led-green": return Kirigami.Theme.positiveTextColor;
                                case "led-yellow": return Kirigami.Theme.highlightColor;
                                case "led-red": return Kirigami.Theme.negativeTextColor;
                                default: return Kirigami.Theme.neutralTextColor;
                                }
                            }
                        }
                        PlasmaComponents.Label {
                            text: MockData.stripProviderSuffix(modelData.providerName)
                            color: Kirigami.Theme.textColor
                            font.bold: true
                            font.pixelSize: PlasmaCore.Units.gridUnit * 0.7
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                        PlasmaComponents.Label {
                            visible: modelData.sourceLabel.length > 0
                            text: modelData.sourceLabel
                            color: Kirigami.Theme.textColor
                            opacity: 0.7
                            font.pixelSize: PlasmaCore.Units.gridUnit * 0.5
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        PlasmaComponents.Label {
                            visible: modelData.statusLabel.length > 0
                            text: modelData.statusLabel
                            color: {
                                switch (modelData.ledClass) {
                                case "led-green": return Kirigami.Theme.positiveTextColor;
                                case "led-yellow": return Kirigami.Theme.highlightColor;
                                case "led-red": return Kirigami.Theme.negativeTextColor;
                                default: return Kirigami.Theme.neutralTextColor;
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
                        color: Kirigami.Theme.negativeTextColor
                        width: parent.width
                        wrapMode: Text.Wrap
                        font.pixelSize: PlasmaCore.Units.gridUnit * 0.55
                    }

                    // ── Plan 区域:按 compactStyle 切换布局 ──
                    Loader {
                        width: parent.width
                        height: planContent.implicitHeight
                        sourceComponent: fullRoot.compactStyle === "pie" ? planRowPie : planRowBar
                    }

                    // 垂直柱状模式(默认)
                    Component {
                        id: planRowBar
                        Column {
                            id: planBarContent
                            width: parent.width
                            spacing: 1
                            visible: modelData.plans.length > 0

                            Repeater {
                                model: modelData.plans
                                delegate: Item {
                                    required property var modelData
                                    width: parent.width
                                    height: planBarRow.implicitHeight + 2

                                    Row {
                                        id: planBarRow
                                        width: parent.width
                                        spacing: 4
                                        leftPadding: 8

                                        // 进度条
                                        Item {
                                            width: parent.width - 220
                                            height: 6
                                            anchors.verticalCenter: parent.verticalCenter

                                            Rectangle {
                                                anchors.fill: parent; radius: 3
                                                color: Qt.rgba(Kirigami.Theme.backgroundColor.r,
                                                               Kirigami.Theme.backgroundColor.g,
                                                               Kirigami.Theme.backgroundColor.b, 0.4)
                                            }
                                            Rectangle {
                                                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                                width: parent.width * (modelData.usedPercent / 100)
                                                radius: 3
                                                color: {
                                                    switch (modelData.barClass) {
                                                    case "bar-green": return Kirigami.Theme.positiveTextColor;
                                                    case "bar-yellow": return Kirigami.Theme.highlightColor;
                                                    case "bar-red": return Kirigami.Theme.negativeTextColor;
                                                    default: return Kirigami.Theme.neutralTextColor;
                                                    }
                                                }
                                                Behavior on width {
                                                    NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                                                }
                                            }
                                        }

                                        // 模板文本
                                        PlasmaComponents.Label {
                                            width: 150
                                            text: i18n(
                                                modelData.template || "%1 限额  %2/%3  重置于 %4",
                                                modelData.planName,
                                                modelData.usedText || modelData.usedPercentLabel,
                                                "",
                                                modelData.resetText
                                            )
                                            color: Kirigami.Theme.textColor
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

                    // 水平饼图模式
                    Component {
                        id: planRowPie
                        Column {
                            id: planPieContent
                            width: parent.width
                            spacing: 1
                            visible: modelData.plans.length > 0

                            // 用一行水平排列多个迷你饼图
                            Row {
                                width: parent.width
                                spacing: 8
                                leftPadding: 8

                                Repeater {
                                    model: modelData.plans
                                    delegate: Item {
                                        required property var modelData
                                        width: 80
                                        height: 60

                                        PieChart {
                                            width: 50
                                            height: 50
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            ringColor: {
                                                switch (modelData.barClass) {
                                                case "bar-green": return Kirigami.Theme.positiveTextColor;
                                                case "bar-yellow": return Kirigami.Theme.highlightColor;
                                                case "bar-red": return Kirigami.Theme.negativeTextColor;
                                                default: return Kirigami.Theme.neutralTextColor;
                                                }
                                            }
                                            remainingColor: Qt.rgba(Kirigami.Theme.backgroundColor.r,
                                                                     Kirigami.Theme.backgroundColor.g,
                                                                     Kirigami.Theme.backgroundColor.b, 0.4)
                                            data: [{
                                                "label": i18n("已用"),
                                                "value": modelData.usedPercent,
                                                "color": {
                                                    switch (modelData.barClass) {
                                                    case "bar-green": return Kirigami.Theme.positiveTextColor;
                                                    case "bar-yellow": return Kirigami.Theme.highlightColor;
                                                    case "bar-red": return Kirigami.Theme.negativeTextColor;
                                                    default: return Kirigami.Theme.neutralTextColor;
                                                    }
                                                }
                                            }, {
                                                "label": i18n("剩余"),
                                                "value": 100 - modelData.usedPercent,
                                                "color": Qt.rgba(Kirigami.Theme.backgroundColor.r,
                                                                 Kirigami.Theme.backgroundColor.g,
                                                                 Kirigami.Theme.backgroundColor.b, 0.4)
                                            }]
                                        }

                                        PlasmaComponents.Label {
                                            anchors.top: parent.bottom
                                            anchors.horizontalCenter: parent.horizontalCenter
                                            text: i18n(
                                                modelData.template || "%1 限额  %2/%3  重置于 %4",
                                                modelData.planName,
                                                modelData.usedText || modelData.usedPercentLabel,
                                                "",
                                                modelData.resetText
                                            )
                                            color: Kirigami.Theme.textColor
                                            opacity: 0.75
                                            font.pixelSize: PlasmaCore.Units.gridUnit * 0.5
                                            elide: Text.ElideRight
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