import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.quickcharts as Charts
import "../js/providerNormalize.js" as ProviderNormalize

Item {
    id: root

    required property var plasmoidItem
    property var providers: []
    property string compactStyle: "pie"
    property int providerIndex: 0
    property bool highlighted: false
    readonly property var currentUsage: ProviderNormalize.providerUsageAt(providers, providerIndex)
    readonly property var tightestUsage: currentUsage
    readonly property real boundedPercent: Math.max(0, Math.min(100,
                                                                 currentUsage.usedPercent))

    implicitWidth: compactStyle === "pie"
        ? Math.max(height, Kirigami.Units.gridUnit * 2)
        : Kirigami.Units.gridUnit * 4
    implicitHeight: Kirigami.Units.gridUnit * 2
    Layout.minimumWidth: compactStyle === "pie"
        ? Kirigami.Units.gridUnit * 2
        : Kirigami.Units.gridUnit * 3
    Layout.preferredWidth: implicitWidth
    clip: true

    Behavior on providerIndex {
        id: providerSwitch
        SequentialAnimation {
            ParallelAnimation {
                PropertyAnimation {
                    target: pieFace
                    property: "scale"
                    from: 0.94
                    to: 1.0
                    duration: 120
                    easing.type: Easing.OutCubic
                }
                PropertyAnimation {
                    target: pieFace
                    property: "opacity"
                    from: 0.85
                    to: 1.0
                    duration: 120
                    easing.type: Easing.OutCubic
                }
            }
            ParallelAnimation {
                PropertyAnimation {
                    target: barFace
                    property: "scale"
                    from: 0.94
                    to: 1.0
                    duration: 120
                    easing.type: Easing.OutCubic
                }
                PropertyAnimation {
                    target: barFace
                    property: "opacity"
                    from: 0.85
                    to: 1.0
                    duration: 120
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Kirigami.Units.cornerRadius
        color: "transparent"
        border.width: root.highlighted ? 2 : 0
        border.color: Qt.rgba(240 / 255, 173 / 255, 78 / 255, 1)
    }

    function usageColor(percent) {
        switch (ProviderNormalize.usageClass(percent, "bar")) {
        case "bar-green": return Kirigami.Theme.positiveTextColor
        case "bar-yellow": return Kirigami.Theme.neutralTextColor
        case "bar-red": return Kirigami.Theme.negativeTextColor
        default: return Kirigami.Theme.disabledTextColor
        }
    }

    Item {
        id: pieFace

        objectName: "compactPie"
        anchors.centerIn: parent
        width: Math.min(root.width, root.height)
        height: width
        visible: root.compactStyle === "pie"

        Charts.PieChart {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.smallSpacing / 2
            valueSources: Charts.SingleValueSource {
                value: root.currentUsage.usedPercent >= 0 ? root.boundedPercent : 0
            }
            colorSource: Charts.SingleValueSource {
                value: root.usageColor(root.currentUsage.usedPercent)
            }
            range {
                from: 0
                to: 100
                automatic: false
            }
            thickness: Math.max(2, Kirigami.Units.smallSpacing)
            backgroundColor: Kirigami.ColorUtils.linearInterpolation(
                                 Kirigami.Theme.backgroundColor,
                                 Kirigami.Theme.textColor, 0.15)
            smoothEnds: true
        }

        QQC2.Label {
            id: piePercent

            objectName: "compactPercent"
            anchors.centerIn: parent
            width: Math.max(1, parent.width - Kirigami.Units.largeSpacing * 2)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: root.currentUsage.usedPercent >= 0
                ? Math.round(root.currentUsage.usedPercent) + "%" : "—"
            color: Kirigami.Theme.textColor
            font.bold: true
            font.pixelSize: Math.max(Kirigami.Theme.smallFont.pixelSize,
                                     parent.width * 0.22)
            minimumPixelSize: Kirigami.Theme.smallFont.pixelSize
            fontSizeMode: Text.Fit
            elide: Text.ElideRight
        }
    }

    RowLayout {
        id: barFace

        objectName: "compactBar"
        anchors.fill: parent
        anchors.leftMargin: Kirigami.Units.smallSpacing
        anchors.rightMargin: Kirigami.Units.smallSpacing
        spacing: Kirigami.Units.smallSpacing
        visible: root.compactStyle === "bar"

        QQC2.Label {
            objectName: "compactBarPercent"
            Layout.minimumWidth: implicitWidth
            text: root.currentUsage.usedPercent >= 0
                ? Math.round(root.currentUsage.usedPercent) + "%" : "—"
            color: Kirigami.Theme.textColor
            font: Kirigami.Theme.smallFont
        }

        QQC2.ProgressBar {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            Layout.minimumHeight: Kirigami.Units.smallSpacing * 2
            Layout.preferredHeight: Kirigami.Units.smallSpacing * 2
            from: 0
            to: 100
            value: root.currentUsage.usedPercent >= 0 ? root.boundedPercent : 0
            Accessible.name: root.currentUsage.providerName
            Accessible.description: qsTr("Used %1%").arg(Math.round(value))
        }
    }

    MouseArea {
        objectName: "compactMouseArea"
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.plasmoidItem.expanded = !root.plasmoidItem.expanded
    }

    Rectangle {
        objectName: "errorBadge"
        visible: Boolean(root.currentUsage.errorText)
        anchors.top: parent.top
        anchors.right: parent.right
        width: Math.max(Kirigami.Units.iconSizes.small, errorLabel.implicitWidth + 6)
        height: width
        radius: width / 2
        color: Kirigami.Theme.negativeTextColor

        QQC2.Label {
            id: errorLabel
            anchors.centerIn: parent
            text: "!"
            color: Kirigami.Theme.backgroundColor
            font.bold: true
        }
    }
}
