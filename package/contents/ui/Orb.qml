import QtQuick
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore

// 圆球组件：显示单个供应商的"已用 %"，三态颜色（绿/黄/红/灰）
// 使用见 contents/ui/main.qml 和将来的 PopupButton.qml
Item {
    id: orbRoot

    property real usedPercent: 0
    property string usedPercentLabel: "—"
    property string ringClass: "orb-gray" // orb-green / orb-yellow / orb-red / orb-gray
    property string providerName: ""

    implicitWidth: PlasmaCore.Units.gridUnit * 3
    implicitHeight: implicitWidth

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: Qt.rgba(0.04, 0.05, 0.1, 0.8)
        border.color: {
            switch (orbRoot.ringClass) {
            case "orb-green":
                return Qt.rgba(0.2, 0.82, 0.6, 0.65);
            case "orb-yellow":
                return Qt.rgba(0.98, 0.75, 0.14, 0.65);
            case "orb-red":
                return Qt.rgba(0.97, 0.44, 0.44, 0.75);
            default:
                return Qt.rgba(1, 1, 1, 0.12);
            }
        }
        border.width: 2
    }

    Text {
        anchors.centerIn: parent
        text: orbRoot.usedPercentLabel
        color: {
            switch (orbRoot.ringClass) {
            case "orb-green":
                return "#34d399";
            case "orb-yellow":
                return "#fbbf24";
            case "orb-red":
                return "#f87171";
            default:
                return "#9ca3af";
            }
        }
        font.pixelSize: PlasmaCore.Units.gridUnit * 0.9
        font.bold: true
    }

    Text {
        anchors.top: parent.bottom
        anchors.topMargin: 2
        anchors.horizontalCenter: parent.horizontalCenter
        text: orbRoot.providerName
        visible: orbRoot.providerName.length > 0
        color: "#cbd5e1"
        font.pixelSize: PlasmaCore.Units.gridUnit * 0.55
    }

}
