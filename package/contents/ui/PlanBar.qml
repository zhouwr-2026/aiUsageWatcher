import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore

// 一条 plan 的水平进度条。
// 行布局：计划名(左) + 进度条(中) + 已用 %(右) + 重置时间(下一行)
Item {
    id: barRoot

    // 数据入口
    property string planName: ""
    property real usedPercent: 0 // 0–100
    property string usedPercentLabel: "—"
    property string barClass: "bar-gray" // bar-green / bar-yellow / bar-red / bar-gray
    property string resetText: "" // 可选：重置时间点；为空不显示
    property string usedText: "" // 可选：已用/总量字符串；如 "141775516 / 180000000"
    property string unitText: "" // 可选：单位；如 "%" / "tokens"
    property string extraText: "" // 可选：自由扩展文本

    implicitHeight: barColumn.implicitHeight
    implicitWidth: 320

    ColumnLayout {
        id: barColumn

        anchors.fill: parent
        spacing: 2

        RowLayout {
            Layout.fillWidth: true
            spacing: PlasmaCore.Units.smallSpacing

            // 计划名（左）
            PlasmaComponents.Label {
                text: barRoot.planName
                color: "#cbd5e1"
                Layout.preferredWidth: 80
                elide: Text.ElideRight
                font.pixelSize: PlasmaCore.Units.gridUnit * 0.65
            }

            // 进度条（中，弹性）
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: PlasmaCore.Units.gridUnit * 0.55
                implicitHeight: PlasmaCore.Units.gridUnit * 0.55

                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: Qt.rgba(1, 1, 1, 0.08)
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: Math.max(2, parent.width * Math.max(0, Math.min(100, barRoot.usedPercent)) / 100)
                    radius: height / 2
                    color: {
                        switch (barRoot.barClass) {
                        case "bar-green":
                            return "#34d399";
                        case "bar-yellow":
                            return "#fbbf24";
                        case "bar-red":
                            return "#f87171";
                        default:
                            return "#6b7280";
                        }
                    }

                    Behavior on width {
                        NumberAnimation {
                            duration: 300
                            easing.type: Easing.OutCubic
                        }

                    }

                }

            }

            // 已用 % + 单位（右）
            PlasmaComponents.Label {
                text: barRoot.unitText.length > 0 ? barRoot.usedPercentLabel + barRoot.unitText : barRoot.usedPercentLabel
                color: {
                    switch (barRoot.barClass) {
                    case "bar-green":
                        return "#34d399";
                    case "bar-yellow":
                        return "#fbbf24";
                    case "bar-red":
                        return "#f87171";
                    default:
                        return "#9ca3af";
                    }
                }
                font.bold: true
                Layout.preferredWidth: 60
                horizontalAlignment: Text.AlignRight
                font.pixelSize: PlasmaCore.Units.gridUnit * 0.65
            }

        }

        // 重置时间 + 已用详情
        RowLayout {
            Layout.fillWidth: true
            spacing: PlasmaCore.Units.smallSpacing
            visible: barRoot.resetText.length > 0 || barRoot.usedText.length > 0 || barRoot.extraText.length > 0

            PlasmaComponents.Label {
                Layout.fillWidth: true
                wrapMode: Text.NoWrap
                elide: Text.ElideRight
                font.pixelSize: PlasmaCore.Units.gridUnit * 0.55
                color: "#94a3b8"
                text: {
                    const parts = [];
                    if (barRoot.resetText.length > 0)
                        parts.push("重置 " + barRoot.resetText);

                    if (barRoot.usedText.length > 0)
                        parts.push(barRoot.usedText);

                    if (barRoot.extraText.length > 0)
                        parts.push(barRoot.extraText);

                    return parts.join(" · ");
                }
            }

        }

    }

}
