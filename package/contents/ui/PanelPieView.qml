pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.quickcharts as Charts
import "../js/mockData.js" as MockData

Flickable {
    id: root

    property var providers: []

    clip: true
    contentWidth: providerRow.implicitWidth
    contentHeight: providerRow.implicitHeight
    boundsBehavior: Flickable.StopAtBounds
    QQC2.ScrollBar.horizontal: QQC2.ScrollBar {}

    function usageColor(percent) {
        switch (MockData.usageClass(percent, "bar")) {
        case "bar-green": return Kirigami.Theme.positiveTextColor
        case "bar-yellow": return Kirigami.Theme.neutralTextColor
        case "bar-red": return Kirigami.Theme.negativeTextColor
        default: return Kirigami.Theme.disabledTextColor
        }
    }

    RowLayout {
        id: providerRow

        spacing: Kirigami.Units.largeSpacing

        Repeater {
            model: root.providers

            delegate: ColumnLayout {
                id: providerColumn

                required property var modelData
                objectName: "panelPieProvider"
                Layout.alignment: Qt.AlignTop
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Heading {
                    Layout.fillWidth: true
                    text: MockData.stripProviderSuffix(providerColumn.modelData.providerName || "")
                    horizontalAlignment: Text.AlignHCenter
                    level: 4
                    elide: Text.ElideRight
                }

                RowLayout {
                    spacing: Kirigami.Units.smallSpacing

                    Repeater {
                        model: providerColumn.modelData.plans || []

                        delegate: ColumnLayout {
                            id: planColumn

                            required property var modelData
                            objectName: "panelPiePlan"
                            spacing: Kirigami.Units.smallSpacing

                            Item {
                                Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                                Layout.preferredHeight: width

                                Charts.PieChart {
                                    anchors.fill: parent
                                    anchors.margins: Kirigami.Units.smallSpacing
                                    valueSources: Charts.SingleValueSource {
                                        value: planColumn.modelData.usedPercent >= 0
                                            ? planColumn.modelData.usedPercent : 0
                                    }
                                    colorSource: Charts.SingleValueSource {
                                        value: root.usageColor(planColumn.modelData.usedPercent)
                                    }
                                    range { from: 0; to: 100; automatic: false }
                                    thickness: Kirigami.Units.smallSpacing * 2
                                    backgroundColor: Kirigami.ColorUtils.linearInterpolation(
                                                         Kirigami.Theme.backgroundColor,
                                                         Kirigami.Theme.textColor, 0.15)
                                    smoothEnds: true
                                }

                                QQC2.Label {
                                    anchors.centerIn: parent
                                    text: planColumn.modelData.usedPercent >= 0
                                        ? planColumn.modelData.usedPercent + "%" : "—"
                                    font.bold: true
                                }
                            }

                            QQC2.Label {
                                Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                                text: planColumn.modelData.planName || qsTr("未命名窗口")
                                horizontalAlignment: Text.AlignHCenter
                                elide: Text.ElideRight
                                font: Kirigami.Theme.smallFont
                            }
                        }
                    }
                }

                QQC2.Label {
                    Layout.fillWidth: true
                    visible: (providerColumn.modelData.plans || []).length === 0
                    text: providerColumn.modelData.errorText
                        || providerColumn.modelData.statusLabel || qsTr("暂无数据")
                    color: providerColumn.modelData.errorText
                        ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.disabledTextColor
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
