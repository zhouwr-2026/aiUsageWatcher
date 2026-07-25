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
    radius: Kirigami.Units.cornerRadius
    color: Kirigami.Theme.alternateBackgroundColor

    ColumnLayout {
        id: content

        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Rectangle {
                Layout.preferredWidth: Kirigami.Units.iconSizes.small
                Layout.preferredHeight: Layout.preferredWidth
                radius: width / 2
                color: "transparent"
                visible: root.logoSource.length > 0 || root.logoChar.length > 0

                Image {
                    id: logoImage
                    anchors.fill: parent
                    anchors.margins: 1
                    source: root.logoSource.length > 0
                        ? (root.logoIsSvg
                           ? "data:image/svg+xml;utf8," + root.logoSource
                           : root.logoSource)
                        : ""
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    visible: status === Image.Ready
                }

                PlasmaComponents.Label {
                    anchors.centerIn: parent
                    visible: logoImage.status !== Image.Ready
                    text: root.logoChar || ""
                    color: Kirigami.Theme.disabledTextColor
                    font: Kirigami.Theme.smallFont
                }
            }

            Item {
                Layout.preferredWidth: Kirigami.Units.smallSpacing
                Layout.preferredHeight: 1
            }

            Rectangle {
                Layout.preferredWidth: Kirigami.Units.smallSpacing * 2
                Layout.preferredHeight: Layout.preferredWidth
                radius: width / 2
                color: root.statusColor(root.ledClass)
            }

            MouseArea {
                id: websiteMouseArea
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                enabled: root._websiteValid(root.website)
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                hoverEnabled: enabled

                PlasmaComponents.Label {
                    objectName: "providerNameLabel"
                    anchors.fill: parent
                    text: root.providerName
                    color: parent.enabled ? Kirigami.Theme.linkColor : Kirigami.Theme.textColor
                    font.bold: true
                    font.underline: parent.enabled
                    elide: Text.ElideRight
                }
                onClicked: Qt.openUrlExternally(root.website)
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
            }
        }
    }
}
