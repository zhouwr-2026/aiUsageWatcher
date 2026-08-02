pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

Rectangle {
    id: root

    property string providerName: ""
    property string ledClass: "led-gray"
    property string sourceLabel: ""
    property string statusLabel: ""
    property var plans: []
    property string errorText: ""
    property string templateText: ""
    property string website: ""
    property string logoSource: ""
    property string logoChar: ""
    property bool logoIsSvg: true
    property string priceText: ""

    function _websiteValid(value) {
        return typeof value === "string" && /^https?:\/\/[^\s]+$/i.test(value)
    }

    function statusColor(statusClass) {
        switch (statusClass) {
        case "led-green": return Kirigami.Theme.positiveTextColor
        case "led-yellow": return Kirigami.Theme.neutralTextColor
        case "led-red": return Kirigami.Theme.negativeTextColor
        default: return Kirigami.Theme.disabledTextColor
        }
    }

    implicitHeight: content.implicitHeight + Kirigami.Units.largeSpacing * 2
    implicitWidth: Kirigami.Units.gridUnit * 16
    color: "transparent"

    RowLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        Rectangle {
            objectName: "providerStatusIndicator"

            readonly property bool isCodex: root.providerName === "Codex"
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Layout.preferredWidth
            Layout.alignment: Qt.AlignVCenter
            radius: width / 2
            color: isCodex ? "white" : "transparent"
            border.width: isCodex ? 1 : 0
            border.color: root.statusColor(root.ledClass)
            Accessible.name: root.providerName
            Accessible.description: root.statusLabel

            Image {
                id: providerLogoImage

                objectName: "providerLogoImage"
                anchors.fill: parent
                anchors.margins: 1
                source: root.logoSource.length > 0
                    ? (root.logoIsSvg
                       ? "data:image/svg+xml;utf8," + root.logoSource
                       : root.logoSource)
                    : ""
                fillMode: Image.PreserveAspectFit
                smooth: true
                asynchronous: true
                visible: status === Image.Ready
            }

            PlasmaComponents.Label {
                objectName: "providerLogoFallback"
                anchors.centerIn: parent
                visible: providerLogoImage.status !== Image.Ready
                text: root.logoChar || ""
                color: Kirigami.Theme.disabledTextColor
                font.bold: true
            }

            Rectangle {
                objectName: "providerStatusBadge"
                width: Math.max(6, Math.round(parent.width / 4))
                height: width
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                radius: width / 2
                color: root.statusColor(root.ledClass)
                border.width: 1
                border.color: Kirigami.Theme.backgroundColor
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                MouseArea {
                    id: websiteMouseArea
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: providerNameLabel.implicitWidth
                    Layout.preferredHeight: providerNameLabel.implicitHeight
                    enabled: root._websiteValid(root.website)
                    cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                    hoverEnabled: enabled

                    PlasmaComponents.Label {
                        id: providerNameLabel
                        objectName: "providerNameLabel"
                        anchors.fill: parent
                        text: root.providerName
                        color: parent.enabled ? Kirigami.Theme.linkColor : Kirigami.Theme.textColor
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    onClicked: Qt.openUrlExternally(root.website)
                }

                PlasmaComponents.Label {
                    objectName: "providerPriceLabel"
                    visible: root.priceText.length > 0
                    text: root.priceText
                    color: Kirigami.Theme.textColor      // 中性色，避免与 LED 状态色混淆
                    font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                    font.bold: true
                    Layout.maximumWidth: Kirigami.Units.gridUnit * 4
                    elide: Text.ElideRight                // 长价格不挤压厂商名称
                }

                Item {
                    Layout.fillWidth: true
                }

                PlasmaComponents.Label {
                    visible: root.sourceLabel.length > 0
                    Layout.maximumWidth: root.width / 4
                    text: root.sourceLabel
                    color: Kirigami.Theme.disabledTextColor
                    font: Kirigami.Theme.smallFont
                    elide: Text.ElideRight
                }

                PlasmaComponents.Label {
                    objectName: "providerStatusLabel"
                    visible: root.statusLabel.length > 0
                    Layout.maximumWidth: root.width / 4
                    text: root.statusLabel
                    color: root.statusColor(root.ledClass)
                    font: Kirigami.Theme.smallFont
                    elide: Text.ElideRight
                }
            }

            PlasmaComponents.Label {
                visible: root.errorText.length > 0
                Layout.fillWidth: true
                text: root.errorText
                color: Kirigami.Theme.negativeTextColor
                font: Kirigami.Theme.smallFont
                wrapMode: Text.Wrap
            }

            Repeater {
                model: root.plans

                delegate: PlanBar {
                    required property var modelData

                    objectName: "planBar"
                    Layout.fillWidth: true
                    planName: modelData.planName || ""
                    usedPercent: modelData.usedPercent
                    usedPercentLabel: modelData.usedPercentLabel || "—"
                    barClass: modelData.barClass || "bar-gray"
                    usedText: modelData.usedText || ""
                    totalText: modelData.totalText || ""
                    unitText: modelData.unitText || ""
                    unitOverflow: modelData.unitOverflow || ""
                    resetText: modelData.resetText || ""
                    extraText: modelData.extraText || ""
                    templateText: modelData.templateText || root.templateText
                    usageSegments: modelData.usageSegments || []
                }
            }
        }
    }
}
