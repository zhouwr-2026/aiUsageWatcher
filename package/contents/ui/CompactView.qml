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
        : Kirigami.Units.gridUnit * 5
    implicitHeight: Kirigami.Units.gridUnit * 2
    Layout.minimumWidth: compactStyle === "pie"
        ? Kirigami.Units.gridUnit * 2
        : Kirigami.Units.gridUnit * 5
    Layout.minimumHeight: implicitHeight
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
        // stale（上次刷新失败）：整图降级灰显，不得以语义色冒充最新数据
        if (root.currentUsage.stale)
            return Kirigami.Theme.disabledTextColor
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

        readonly property real thickness: Math.max(2, Kirigami.Units.smallSpacing)

        Charts.PieChart {
            objectName: "compactPieChart"
            anchors.fill: parent
            anchors.margins: Kirigami.Units.smallSpacing / 2
            valueSources: Charts.SingleValueSource {
                objectName: "compactPieValueSource"
                value: root.currentUsage.usedPercent >= 0 ? root.boundedPercent : 0
            }
            colorSource: Charts.SingleValueSource {
                objectName: "compactPieColorSource"
                value: root.usageColor(root.currentUsage.usedPercent)
            }
            range { from: 0; to: 100; automatic: false }
            thickness: pieFace.thickness
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
            textFormat: Text.PlainText
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
            // 无数据时显示占位符 · 有数据时显示百分比
            text: {
                const pct = root.currentUsage.usedPercent
                const name = root.currentUsage.providerName || ""
                if (pct >= 0) return Math.round(pct) + "%"
                if (name) return name.slice(0, 2)  // 用供应商名前两字作占位
                return "—"
            }
            color: Kirigami.Theme.textColor
            font: Kirigami.Theme.smallFont
            textFormat: Text.PlainText   // providerName 前两字可能来自用户输入
        }

        // 与 Plasma 硬盘监控一致：使用 ProgressBar，但完全覆写内容和背景，
        // 避免依赖 Breeze 默认样式并确保 0% 时底轨仍可见。
        QQC2.ProgressBar {
            id: compactProgress

            objectName: "compactProgressBar"
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            Layout.minimumWidth: Kirigami.Units.gridUnit * 2
            from: 0
            to: 100
            value: root.currentUsage.usedPercent >= 0 ? root.boundedPercent : 0
            topPadding: topInset
            bottomPadding: bottomInset
            Accessible.name: root.currentUsage.providerName
            Accessible.description: qsTr("Used %1%").arg(
                                            Math.round(root.boundedPercent))

            contentItem: Item {
                Rectangle {
                    objectName: "compactProgressFill"
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: Math.max(parent.width * compactProgress.visualPosition,
                                    root.currentUsage.usedPercent >= 0 ? height : 0)
                    radius: height / 2
                    color: root.usageColor(root.currentUsage.usedPercent)

                    Behavior on width {
                        NumberAnimation {
                            duration: 300
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }

            background: Rectangle {
                objectName: "compactBarTrack"
                implicitWidth: 100
                implicitHeight: Kirigami.Units.largeSpacing
                radius: height / 2
                color: Kirigami.ColorUtils.linearInterpolation(
                    Kirigami.Theme.backgroundColor,
                    Kirigami.Theme.textColor, 0.2)
            }
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
