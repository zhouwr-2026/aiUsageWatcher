import QtQuick
import org.kde.kirigami as Kirigami
import "../js/mockData.js" as MockData

Item {
    id: root

    required property var plasmoidItem
    property var providers: []
    property string compactStyle: "pie"
    property int providerIndex: 0
    readonly property var currentUsage: MockData.providerUsageAt(providers, providerIndex)
    readonly property var tightestUsage: currentUsage

    implicitWidth: Kirigami.Units.gridUnit * 3
    implicitHeight: Kirigami.Units.gridUnit * 3

    function usageColor(percent) {
        switch (MockData.usageClass(percent, "bar")) {
        case "bar-green": return Kirigami.Theme.positiveTextColor
        case "bar-yellow": return Kirigami.Theme.neutralTextColor
        case "bar-red": return Kirigami.Theme.negativeTextColor
        default: return Kirigami.Theme.disabledTextColor
        }
    }

    // 底色圆形 + 边框(在所有样式下都显示)
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: Qt.rgba(Kirigami.Theme.backgroundColor.r,
                       Kirigami.Theme.backgroundColor.g,
                       Kirigami.Theme.backgroundColor.b, 0.85)
        border.width: 2
        border.color: root.usageColor(root.currentUsage.usedPercent)
        opacity: 0.95
    }

    // pie 模式:内嵌 PieChart
    PieChart {
        objectName: "compactPie"
        anchors.fill: parent
        anchors.margins: Kirigami.Units.smallSpacing
        visible: root.compactStyle === "pie"
        ringColor: root.usageColor(root.currentUsage.usedPercent)
        remainingColor: Qt.rgba(Kirigami.Theme.backgroundColor.r,
                                Kirigami.Theme.backgroundColor.g,
                                Kirigami.Theme.backgroundColor.b, 0.4)
        segments: {
            const percent = root.currentUsage.usedPercent
            const used = Math.max(0, Math.min(100, percent))
            return [{
                "label": "已用",
                "value": used,
                "color": root.usageColor(percent)
            }, {
                "label": "剩余",
                "value": Math.max(0, 100 - used),
                "color": Qt.rgba(Kirigami.Theme.backgroundColor.r,
                                 Kirigami.Theme.backgroundColor.g,
                                 Kirigami.Theme.backgroundColor.b, 0.4)
            }];
        }
    }

    // bar 模式:水平填充矩形(用作柱状图)
    Rectangle {
        id: barRect
        objectName: "compactBar"
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
        }
        anchors.leftMargin: Kirigami.Units.smallSpacing
        anchors.rightMargin: Kirigami.Units.smallSpacing
        height: parent.height * 0.18
        radius: height / 2
        visible: root.compactStyle === "bar"
        color: Qt.rgba(Kirigami.Theme.backgroundColor.r,
                       Kirigami.Theme.backgroundColor.g,
                       Kirigami.Theme.backgroundColor.b, 0.4)

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * (Math.max(0, Math.min(100,
                                                       root.currentUsage.usedPercent)) / 100)
            radius: parent.radius
            color: root.usageColor(root.currentUsage.usedPercent)
            Behavior on width {
                NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
            }
        }
    }

    // 中心整数百分比文字
    Text {
        objectName: "compactPercent"
        anchors.centerIn: parent
        text: root.currentUsage.usedPercent >= 0
            ? Math.round(root.currentUsage.usedPercent) + "%" : "—"
        color: root.usageColor(root.currentUsage.usedPercent)
        font.pixelSize: Kirigami.Units.gridUnit * 1.2
        font.bold: true
    }

    // 点击切换弹窗
    MouseArea {
        objectName: "compactMouseArea"
        anchors.fill: parent
        hoverEnabled: true
        z: 100
        property bool wasExpanded: false
        onPressed: wasExpanded = root.plasmoidItem.expanded
        onClicked: root.plasmoidItem.expanded = !wasExpanded
    }
}
