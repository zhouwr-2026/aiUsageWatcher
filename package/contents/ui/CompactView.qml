import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.quickcharts as Charts
import "../js/mockData.js" as MockData

Item {
    id: root

    required property var plasmoidItem
    property var providers: []
    property string compactStyle: "pie"
    property int providerIndex: 0
    readonly property var currentUsage: MockData.providerUsageAt(providers, providerIndex)
    readonly property var tightestUsage: currentUsage
    readonly property real boundedPercent: Math.max(0, Math.min(100,
                                                                 currentUsage.usedPercent))

    implicitWidth: compactStyle === "pie"
        ? Math.max(height, Kirigami.Units.gridUnit)
        : Kirigami.Units.gridUnit * 4
    implicitHeight: Kirigami.Units.gridUnit
    Layout.minimumWidth: compactStyle === "pie"
        ? Math.max(height, Kirigami.Units.gridUnit)
        : Kirigami.Units.gridUnit * 3
    Layout.preferredWidth: implicitWidth
    clip: true

    function usageColor(percent) {
        switch (MockData.usageClass(percent, "bar")) {
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
}
