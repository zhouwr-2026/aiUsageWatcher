import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore

// 一个供应商：标题 + 状态 + 多条 PlanBar 堆叠
// 仿 KDE 磁盘使用率小部件的"每个磁盘一段"样式
Rectangle {
    id: groupRoot

    // 数据入口
    property string providerName: ""
    property string ledClass: "led-gray" // led-green / led-yellow / led-red / led-gray
    property string sourceLabel: "" // "自定义" / "余额" / "套餐" / "本地"
    property string statusLabel: "" // "可用" / "降级" / "异常" / "未配置"
    property var plans: [] // [{planName, usedPercent, usedPercentLabel, barClass, resetText, usedText, unitText, extraText}]
    property string errorText: "" // 当 status=error 时显示

    implicitHeight: column.implicitHeight + 2 * PlasmaCore.Units.smallSpacing
    radius: PlasmaCore.Units.smallSpacing
    color: Qt.rgba(0.04, 0.05, 0.1, 0.55)
    border.width: 1
    // 整体边框颜色随状态切换
    border.color: {
        switch (ledClass) {
        case "led-green":
            return Qt.rgba(0.2, 0.82, 0.6, 0.2);
        case "led-yellow":
            return Qt.rgba(0.98, 0.75, 0.14, 0.2);
        case "led-red":
            return Qt.rgba(0.97, 0.44, 0.44, 0.3);
        default:
            return Qt.rgba(1, 1, 1, 0.08);
        }
    }

    ColumnLayout {
        id: column

        anchors.fill: parent
        anchors.margins: PlasmaCore.Units.smallSpacing
        spacing: PlasmaCore.Units.smallSpacing

        // 标题行：LED 灯 + 供应商名 + 来源标签 + 状态标签
        RowLayout {
            Layout.fillWidth: true
            spacing: PlasmaCore.Units.smallSpacing

            Rectangle {
                width: PlasmaCore.Units.gridUnit * 0.45
                height: width
                radius: width / 2
                color: {
                    switch (groupRoot.ledClass) {
                    case "led-green":
                        return "#34d399";
                    case "led-yellow":
                        return "#fbbf24";
                    case "led-red":
                        return "#f87171";
                    default:
                        return "#6b7280";
                    }
                }
            }

            PlasmaComponents.Label {
                text: groupRoot.providerName
                color: "#f1f5f9"
                font.bold: true
                Layout.fillWidth: true
                elide: Text.ElideRight
                font.pixelSize: PlasmaCore.Units.gridUnit * 0.75
            }

            PlasmaComponents.Label {
                visible: groupRoot.sourceLabel.length > 0
                text: groupRoot.sourceLabel
                color: "#94a3b8"
                font.pixelSize: PlasmaCore.Units.gridUnit * 0.55
            }

            PlasmaComponents.Label {
                visible: groupRoot.statusLabel.length > 0
                text: groupRoot.statusLabel
                color: {
                    switch (groupRoot.ledClass) {
                    case "led-green":
                        return "#34d399";
                    case "led-yellow":
                        return "#fbbf24";
                    case "led-red":
                        return "#f87171";
                    default:
                        return "#9ca3af";
                    }
                }
                font.pixelSize: PlasmaCore.Units.gridUnit * 0.55
            }

        }

        // 错误信息行
        PlasmaComponents.Label {
            visible: groupRoot.errorText.length > 0
            text: groupRoot.errorText
            color: "#fca5a5"
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: PlasmaCore.Units.gridUnit * 0.6
        }

        // 多条 PlanBar 堆叠
        Repeater {
            model: groupRoot.plans

            delegate: PlanBar {
                Layout.fillWidth: true
                planName: modelData.planName
                usedPercent: modelData.usedPercent
                usedPercentLabel: modelData.usedPercentLabel
                barClass: modelData.barClass
                resetText: modelData.resetText
                usedText: modelData.usedText
                unitText: modelData.unitText
                extraText: modelData.extraText
            }

        }

    }

}
