pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import org.kde.quickcharts as Charts
import "../js/providerNormalize.js" as ProviderNormalize

Flickable {
    id: root

    property var providers: []

    clip: true
    contentWidth: providerRow.implicitWidth
    contentHeight: providerRow.implicitHeight
    boundsBehavior: Flickable.StopAtBounds
    QQC2.ScrollBar.horizontal: QQC2.ScrollBar {}

    function usageColor(percent) {
        switch (ProviderNormalize.usageClass(percent, "bar")) {
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
                    text: ProviderNormalize.stripProviderSuffix(providerColumn.modelData.providerName || "")
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

                            readonly property var usageSegments: ProviderNormalize.copyUsageSegments(modelData.usageSegments)
                            readonly property bool hasUsageSegments: usageSegments.length > 0
                            readonly property var segmentPercents: usageSegments.map(segment => segment.usedPercent)
                            readonly property var segmentColors: usageSegments.map(segment =>
                                segment.kind === "today" ? Kirigami.Theme.positiveTextColor
                                                         : Kirigami.Theme.highlightColor)

                            Item {
                                id: pieContainer

                                objectName: "panelPieChart"
                                Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                                Layout.preferredHeight: width

                                readonly property real chartMargin: Kirigami.Units.smallSpacing
                                readonly property real outerRadius: Math.max(0, Math.min(width, height) / 2 - chartMargin)
                                readonly property real ringThickness: Kirigami.Units.smallSpacing * 2
                                readonly property bool hasUsageSegments: planColumn.hasUsageSegments
                                property int hoveredSegment: -1

                                // 无障碍：两段名称/百分比/金额均可读，不靠颜色区分（设计文档要求）
                                Accessible.name: planColumn.hasUsageSegments
                                    ? planColumn.usageSegments.map(ProviderNormalize.usageSegmentLabel).join("，")
                                    : (planColumn.modelData.usedPercent >= 0
                                        ? qsTr("已用 %1%").arg(Math.round(planColumn.modelData.usedPercent))
                                        : qsTr("无数据"))

                                function usageSegmentAt(x, y) {
                                    if (!planColumn.hasUsageSegments)
                                        return -1
                                    const deltaX = x - width / 2
                                    const deltaY = y - height / 2
                                    const distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY)
                                    if (distance > outerRadius || distance < outerRadius - ringThickness)
                                        return -1
                                    const angle = (Math.atan2(deltaX, -deltaY) * 180 / Math.PI + 360) % 360
                                    let endAngle = 0
                                    for (let segmentIndex = 0; segmentIndex < planColumn.usageSegments.length; ++segmentIndex) {
                                        endAngle += planColumn.usageSegments[segmentIndex].usedPercent * 3.6
                                        if (angle < endAngle)
                                            return segmentIndex
                                    }
                                    return -1
                                }

                                Charts.ArraySource {
                                    id: segmentValues

                                    array: planColumn.segmentPercents
                                }

                                Charts.ArraySource {
                                    id: segmentColors

                                    array: planColumn.segmentColors
                                }

                                Charts.SingleValueSource {
                                    id: usedValue

                                    value: planColumn.modelData.usedPercent >= 0
                                        ? planColumn.modelData.usedPercent : 0
                                }

                                Charts.SingleValueSource {
                                    id: usedColor

                                    value: root.usageColor(planColumn.modelData.usedPercent)
                                }

                                Charts.PieChart {
                                    anchors.fill: parent
                                    anchors.margins: Kirigami.Units.smallSpacing
                                    fromAngle: -90
                                    toAngle: 270
                                    valueSources: planColumn.hasUsageSegments ? segmentValues : usedValue
                                    colorSource: planColumn.hasUsageSegments ? segmentColors : usedColor
                                    range { from: 0; to: 100; automatic: false }
                                    thickness: Kirigami.Units.smallSpacing * 2
                                    backgroundColor: Kirigami.ColorUtils.linearInterpolation(
                                                         Kirigami.Theme.backgroundColor,
                                                         Kirigami.Theme.textColor, 0.15)
                                    smoothEnds: true
                                }

                                HoverHandler {
                                    id: pieHover

                                    onPointChanged: pieContainer.hoveredSegment = pieContainer.usageSegmentAt(
                                        point.position.x, point.position.y)
                                    onActiveChanged: {
                                        if (!active)
                                            pieContainer.hoveredSegment = -1
                                    }
                                }

                                PlasmaComponents.ToolTip {
                                    text: pieContainer.hoveredSegment >= 0
                                        ? ProviderNormalize.usageSegmentLabel(planColumn.usageSegments[pieContainer.hoveredSegment])
                                        : ""
                                    visible: pieHover.hovered && pieContainer.hoveredSegment >= 0
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
