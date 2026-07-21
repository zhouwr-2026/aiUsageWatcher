import QtQuick
import org.kde.plasma.core as PlasmaCore
import "../js/mockData.js" as MockData

Item {
    id: root

    property var providers: []
    property string compactStyle: "pie"  // "pie" | "bar"

    implicitWidth: PlasmaCore.Units.gridUnit * 3
    implicitHeight: PlasmaCore.Units.gridUnit * 3

    // 取最紧张供应商的 usedPercent,无数据返回 -1
    function tightestPercent() {
        let worst = -1;
        for (const p of providers) {
            for (const plan of p.plans) {
                if (typeof plan.usedPercent === "number" && plan.usedPercent > worst)
                    worst = plan.usedPercent;
            }
        }
        return worst;
    }

    // 阈值色(PlasmaCore.Theme)
    function barColor(pct) {
        if (pct < 0) return PlasmaCore.Theme.NeutralText;
        if (pct <= 5) return PlasmaCore.Theme.NegativeText;
        if (pct <= 15) return PlasmaCore.Theme.HighlightColor;
        return PlasmaCore.Theme.PositiveText;
    }

    // 底色圆形 + 边框(在所有样式下都显示)
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: Qt.rgba(PlasmaCore.Theme.backgroundColor.r,
                       PlasmaCore.Theme.backgroundColor.g,
                       PlasmaCore.Theme.backgroundColor.b, 0.85)
        border.width: 2
        border.color: root.barColor(root.tightestPercent())
        opacity: 0.95
    }

    // pie 模式:内嵌 PieChart
    PieChart {
        anchors.fill: parent
        anchors.margins: 4
        visible: root.compactStyle === "pie"
        ringColor: root.barColor(root.tightestPercent())
        remainingColor: Qt.rgba(PlasmaCore.Theme.backgroundColor.r,
                                PlasmaCore.Theme.backgroundColor.g,
                                PlasmaCore.Theme.backgroundColor.b, 0.4)
        data: {
            const worst = root.tightestPercent();
            const used = Math.max(0, worst);
            return [{
                "label": "已用",
                "value": used,
                "color": root.barColor(worst)
            }, {
                "label": "剩余",
                "value": Math.max(0, 100 - used),
                "color": Qt.rgba(PlasmaCore.Theme.backgroundColor.r,
                                 PlasmaCore.Theme.backgroundColor.g,
                                 PlasmaCore.Theme.backgroundColor.b, 0.4)
            }];
        }
    }

    // bar 模式:水平填充矩形(用作柱状图)
    Rectangle {
        id: barRect
        anchors {
            left: parent.left
            right: parent.right
            verticalCenter: parent.verticalCenter
        }
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        height: parent.height * 0.18
        radius: height / 2
        visible: root.compactStyle === "bar"
        color: Qt.rgba(PlasmaCore.Theme.backgroundColor.r,
                       PlasmaCore.Theme.backgroundColor.g,
                       PlasmaCore.Theme.backgroundColor.b, 0.4)

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * (Math.max(0, root.tightestPercent()) / 100)
            radius: parent.radius
            color: root.barColor(root.tightestPercent())
            Behavior on width {
                NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
            }
        }
    }

    // 中心整数百分比文字
    Text {
        anchors.centerIn: parent
        text: {
            const p = root.tightestPercent();
            return p >= 0 ? Math.round(p) + "%" : "—";
        }
        color: root.barColor(root.tightestPercent())
        font.pixelSize: PlasmaCore.Units.gridUnit * 1.2
        font.bold: true
    }

    // 点击切换弹窗
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        z: 100
        onClicked: plasmoid.expanded = !plasmoid.expanded
    }
}