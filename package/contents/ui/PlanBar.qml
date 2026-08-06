pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import "../js/providerNormalize.js" as ProviderNormalize

Item {
    id: root

    property string planName: ""
    property real usedPercent: -1
    property string usedPercentLabel: "—"
    property string barClass: "bar-gray"
    property string usedText: ""
    property string totalText: ""
    property string unitText: ""
    property string unitOverflow: ""
    property string resetText: ""
    property string extraText: ""
    property string templateText: "%1 限额  %2/%3  重置于 %4"
    property var usageSegments: []

    readonly property var normalizedUsageSegments: ProviderNormalize.copyUsageSegments(usageSegments)
    readonly property bool hasUsageSegments: normalizedUsageSegments.length > 0

    readonly property string renderedTemplate: renderTemplate(templateText, [
        planName, usedText, totalText, resetText
    ])

    readonly property string fullDetailText: [unitText, unitOverflow, extraText]
        .filter(value => value.length > 0).join(" · ")
    readonly property string fullDetailTooltipText: fullDetailText.split(" | ").join("\n")

    function usageColor(usageClass) {
        switch (usageClass) {
        case "bar-green": return Kirigami.Theme.positiveTextColor
        case "bar-yellow": return Kirigami.Theme.neutralTextColor
        case "bar-red": return Kirigami.Theme.negativeTextColor
        default: return Kirigami.Theme.disabledTextColor
        }
    }

    function segmentColor(segment) {
        if (!segment)
            return Kirigami.Theme.disabledTextColor
        return segment.kind === "today"
            ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.highlightColor
    }

    function renderTemplate(template, values) {
        let rendered = template || "%1 限额  %2/%3  重置于 %4"
        // planName 自带"限额"时去掉模板固定后缀；精确作用于 %1 占位，不误伤数据串
        if (values.length > 0 && String(values[0]).indexOf("限额") >= 0)
            rendered = rendered.replace(/%1\s*限额/, "%1")
        for (let i = 0; i < values.length; ++i) {
            const value = values[i] === undefined || values[i] === null ? "" : String(values[i])
            rendered = rendered.split("%" + (i + 1)).join(value)
        }
        return rendered
    }

    implicitHeight: content.implicitHeight
    implicitWidth: Kirigami.Units.gridUnit * 16

    ColumnLayout {
        id: content

        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents.Label {
                Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                Layout.maximumWidth: Kirigami.Units.gridUnit * 7
                Layout.minimumWidth: 0
                text: root.planName
                color: Kirigami.Theme.textColor
                wrapMode: Text.Wrap
                elide: Text.ElideNone
            }

            QQC2.ProgressBar {
                id: planProgress

                objectName: "planProgressBar"
                Layout.fillWidth: true
                Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                from: 0
                to: 100
                value: root.usedPercent >= 0
                    ? Math.max(0, Math.min(100, root.usedPercent)) : 0
                topPadding: topInset
                bottomPadding: bottomInset

                contentItem: Item {
                    Rectangle {
                        objectName: "planProgressFill"
                        visible: !root.hasUsageSegments
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        // 有数据但用量为 0% 时给最小可见宽度，避免进度条被误认为"无数据"（全灰轨道）
                        width: Math.max(parent.width * planProgress.visualPosition,
                                        root.usedPercent >= 0 ? height : 0)
                        radius: height / 2
                        color: root.usageColor(root.barClass)

                        Behavior on width {
                            NumberAnimation {
                                duration: 300
                                easing.type: Easing.OutCubic
                            }
                        }
                    }

                    Rectangle {
                        id: segmentedFill

                        visible: root.hasUsageSegments
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: Math.max(parent.width * planProgress.visualPosition,
                                        root.usedPercent >= 0 ? height : 0)
                        radius: height / 2
                        color: root.segmentColor(lastSegment)
                        objectName: "usageCurrentSegment"
                        Accessible.name: ProviderNormalize.usageSegmentLabel(lastSegment)

                        readonly property var firstSegment: root.normalizedUsageSegments[0]
                        readonly property var lastSegment: root.normalizedUsageSegments[
                            root.normalizedUsageSegments.length - 1]
                        // 分母为 0（用量 <0.5% 被取整）时回退 0，避免 Infinity 撑爆进度条
                        readonly property real previousWidth: root.normalizedUsageSegments.length > 1 && root.usedPercent > 0
                            ? width * firstSegment.usedPercent / root.usedPercent : 0

                        Item {
                            id: previousSegment

                            objectName: "usagePreviousSegment"
                            visible: segmentedFill.previousWidth > 0
                            width: segmentedFill.previousWidth
                            height: parent.height
                            clip: true
                            Accessible.name: ProviderNormalize.usageSegmentLabel(segmentedFill.firstSegment)

                            Rectangle {
                                objectName: "usagePreviousSegmentShape"
                                width: Math.max(parent.width, parent.height)
                                height: parent.height
                                radius: height / 2
                                color: root.segmentColor(segmentedFill.firstSegment)

                                Rectangle {
                                    anchors.right: parent.right
                                    width: Math.min(parent.height / 2, parent.width)
                                    height: parent.height
                                    color: parent.color
                                }
                            }

                            HoverHandler {
                                id: startCapHover
                            }

                            PlasmaComponents.ToolTip {
                                text: ProviderNormalize.usageSegmentLabel(segmentedFill.firstSegment)
                                visible: startCapHover.hovered
                            }
                        }

                        HoverHandler {
                            id: endCapHover
                            enabled: !startCapHover.hovered
                        }

                        PlasmaComponents.ToolTip {
                            text: ProviderNormalize.usageSegmentLabel(segmentedFill.lastSegment)
                            visible: endCapHover.hovered
                        }
                    }
                }

                background: Rectangle {
                    objectName: "unusedTrack"
                    implicitWidth: 100
                    implicitHeight: Kirigami.Units.largeSpacing
                    radius: height / 2
                    color: Kirigami.ColorUtils.linearInterpolation(
                        Kirigami.Theme.backgroundColor,
                        Kirigami.Theme.textColor, 0.2)
                }
            }

            PlasmaComponents.Label {
                objectName: "planUsedPercentLabel"
                Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                text: root.usedPercentLabel
                color: root.usageColor(root.barClass)
                font.bold: true
                horizontalAlignment: Text.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents.Label {
                objectName: "templateTextLabel"

                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.renderedTemplate
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight
            }

            PlasmaComponents.Label {
                objectName: "extraTextLabel"
                visible: text.length > 0
                Layout.maximumWidth: root.width / 3
                text: root.fullDetailText
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight

                HoverHandler {
                    id: extraTextHover
                }

                // 使用独立弹层越过面板边界，避免末行提示被自动翻到列表上方。
                PlasmaComponents.ToolTip {
                    id: extraTextTooltip
                    objectName: "extraTextTooltip"
                    popupType: QQC2.Popup.Window
                    readonly property real preferredY: parent.height + Kirigami.Units.smallSpacing
                    x: 0
                    y: preferredY
                    text: root.extraText.length > 0
                        ? root.extraText.split(" | ").join("\n")
                        : ""
                    visible: extraTextHover.hovered && text.length > 0
                }
            }
        }
    }
}
