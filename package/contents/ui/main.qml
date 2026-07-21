import QtQuick
import QtQuick.Layouts
import org.kde.plasma.components as PlasmaComponents
import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import "../js/mockData.js" as MockData

PlasmoidItem {
    id: root

    // ── 配置绑定 ─────────────────────────────────────────
    property string displayStyle: plasmoid.configuration.displayStyle || "pie"
    readonly property var themeColors: {
        "positive": PlasmaCore.Theme.PositiveText,
        "neutral": PlasmaCore.Theme.NeutralText,
        "negative": PlasmaCore.Theme.NegativeText,
        "highlight": PlasmaCore.Theme.HighlightColor,
        "bg": Qt.rgba(PlasmaCore.Theme.backgroundColor.r,
                     PlasmaCore.Theme.backgroundColor.g,
                     PlasmaCore.Theme.backgroundColor.b, 0.92)
    }

    // ── 数据 ─────────────────────────────────────────
    property var providers: MockData.SEED_PROVIDERS
    property var history: MockData.HISTORY_BUFFER

    Plasmoid.title: ""
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground
    toolTipMainText: i18n("AI 用量监控")
    toolTipSubText: {
        const worst = root.tightestUsedPercent();
        const name = root.tightestProviderName();
        if (providers.length === 0 || worst < 0)
            return name.length > 0 ? name + ": " + i18n("无数据") : i18n("暂无供应商数据");
        return name + ": " + Math.round(worst) + "%";
    }

    // ── 右键菜单 ─────────────────────────────────────────
    // 注意:PlasmaCore.Action 不支持嵌套子菜单(没有 default property 接 children)。
    // 把"显示样式"拍平为 3 个顶级 action,前面加图标前缀区分。
    Plasmoid.contextualActions: [
        PlasmaCore.Action {
            text: i18n("打开系统监视器…")
            icon.name: "utilities-system-monitor"
            onTriggered: Qt.openUrlExternally("plasma-systemmonitor")
        },
        PlasmaCore.Action {
            text: i18n("配置…")
            icon.name: "configure"
            onTriggered: plasmoid.action("configure").trigger()
        },
        PlasmaCore.Action {
            text: i18n("样式:饼状图")
            icon.name: "office-chart-pie"
            checkable: true
            checked: root.displayStyle === "pie"
            onTriggered: plasmoid.configuration.displayStyle = "pie"
        },
        PlasmaCore.Action {
            text: i18n("样式:柱状图")
            icon.name: "office-chart-bar"
            checkable: true
            checked: root.displayStyle === "bar"
            onTriggered: plasmoid.configuration.displayStyle = "bar"
        },
        PlasmaCore.Action {
            text: i18n("样式:传感器详情")
            icon.name: "view-object-historic-linear"
            checkable: true
            checked: root.displayStyle === "sensor"
            onTriggered: plasmoid.configuration.displayStyle = "sensor"
        },
        PlasmaCore.Action {
            text: i18n("刷新")
            icon.name: "view-refresh"
            onTriggered: root.providers = MockData.fluctuateProviders(root.providers)
        }
    ]

    function tightestUsedPercent() {
        let worst = -1;
        for (const p of providers) {
            for (const plan of p.plans) {
                if (typeof plan.usedPercent === "number" && plan.usedPercent > worst)
                    worst = plan.usedPercent;
            }
        }
        return worst;
    }

    function tightestProviderName() {
        const worst = tightestUsedPercent();
        if (worst < 0) return "";
        for (const p of providers) {
            for (const plan of p.plans) {
                if (plan.usedPercent === worst) return MockData.stripProviderSuffix(p.providerName);
            }
        }
        return "";
    }

    function barColor(pct) {
        if (pct < 0) return themeColors.neutral;
        if (pct <= 5) return themeColors.negative;
        if (pct <= 15) return themeColors.highlight;
        return themeColors.positive;
    }

    Timer {
        interval: 60000
        running: true
        repeat: true
        onTriggered: root.providers = MockData.fluctuateProviders(root.providers)
    }

    // ── Compact：圆球 ──────────────────────────────────
    compactRepresentation: Item {
        id: compactRoot

        implicitWidth: PlasmaCore.Units.gridUnit * 3
        implicitHeight: PlasmaCore.Units.gridUnit * 3

        // 底色圆形
        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: Qt.rgba(PlasmaCore.Theme.backgroundColor.r,
                          PlasmaCore.Theme.backgroundColor.g,
                          PlasmaCore.Theme.backgroundColor.b, 0.85)
            border.width: 2
            border.color: root.barColor(root.tightestUsedPercent())
            opacity: 0.95
        }

        // 饼状图(仅 displayStyle === "pie" 时显示)
        PieChart {
            id: orbPie
            anchors.fill: parent
            anchors.margins: 4
            visible: root.displayStyle === "pie"
            ringColor: root.themeColors.highlight
            remainingColor: Qt.rgba(PlasmaCore.Theme.backgroundColor.r,
                                    PlasmaCore.Theme.backgroundColor.g,
                                    PlasmaCore.Theme.backgroundColor.b, 0.4)
            data: [{
                "label": i18n("已用"),
                "value": Math.max(0, root.tightestUsedPercent()),
                "color": root.barColor(root.tightestUsedPercent())
            }, {
                "label": i18n("剩余"),
                "value": Math.max(0, 100 - Math.max(0, root.tightestUsedPercent())),
                "color": Qt.rgba(PlasmaCore.Theme.backgroundColor.r,
                                 PlasmaCore.Theme.backgroundColor.g,
                                 PlasmaCore.Theme.backgroundColor.b, 0.4)
            }]
        }

        // 柱状图(显示为彩色填充圆)
        Rectangle {
            id: orbBar
            anchors.centerIn: parent
            width: parent.width * 0.7
            height: parent.height * (Math.max(0, root.tightestUsedPercent()) / 100)
            radius: width / 2
            color: root.barColor(root.tightestUsedPercent())
            visible: root.displayStyle === "bar"
            Behavior on height { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        }

        // 传感器详情(圆球内显示折线小图)
        LineChart {
            id: orbSensor
            anchors.fill: parent
            anchors.margins: 6
            visible: root.displayStyle === "sensor"
            lineColor: root.themeColors.highlight
            fillOpacity: 0.3
            lineWidth: 1.5
            values: {
                const arr = [];
                for (let i = 0; i < root.history.length; ++i)
                    arr.push(root.history[i].worstPercent);
                return arr;
            }
        }

        // 中心百分比文字(饼状/柱状时显示;传感器详情时不显示)
        Text {
            anchors.centerIn: parent
            visible: root.displayStyle !== "sensor"
            text: {
                const p = root.tightestUsedPercent();
                return p >= 0 ? Math.round(p) + "%" : "—";
            }
            color: root.barColor(root.tightestUsedPercent())
            font.pixelSize: PlasmaCore.Units.gridUnit * 1.2
            font.bold: true
        }

        // 兜底点击区域
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            z: 100
            onClicked: plasmoid.expanded = !plasmoid.expanded
        }
    }

    // ── Full：弹出面板 ──────────────────────────────────
    fullRepresentation: Item {
        id: fullRoot

        Layout.minimumWidth: PlasmaCore.Units.gridUnit * 32
        Layout.minimumHeight: PlasmaCore.Units.gridUnit * 4
        Layout.preferredWidth: PlasmaCore.Units.gridUnit * 36
        Layout.preferredHeight: PlasmaCore.Units.gridUnit * 24

        Rectangle {
            anchors.fill: parent
            radius: PlasmaCore.Units.smallSpacing
            color: root.themeColors.bg
            border.color: Qt.rgba(1, 1, 1, 0.08)
            border.width: 1
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: PlasmaCore.Units.gridUnit * 0.5
            spacing: PlasmaCore.Units.smallSpacing

            // 标题栏
            RowLayout {
                Layout.fillWidth: true
                spacing: PlasmaCore.Units.smallSpacing

                PlasmaComponents.Label {
                    text: i18n("AI 用量监控")
                    color: PlasmaCore.Theme.TextColor
                    font.bold: true
                    font.pixelSize: PlasmaCore.Units.gridUnit * 0.85
                    Layout.fillWidth: true
                }

                // 当前样式指示
                PlasmaComponents.Label {
                    text: {
                        switch (root.displayStyle) {
                        case "pie": return i18n("饼状图");
                        case "bar": return i18n("柱状图");
                        case "sensor": return i18n("传感器详情");
                        }
                        return "";
                    }
                    color: PlasmaCore.Theme.TextColor
                    font.pixelSize: PlasmaCore.Units.gridUnit * 0.5
                    opacity: 0.7
                }

                PlasmaComponents.ToolButton {
                    icon.name: "configure"
                    onClicked: plasmoid.action("configure").trigger()
                }

                PlasmaComponents.ToolButton {
                    icon.name: "window-pin"
                    checkable: true
                    checked: false
                    onCheckedChanged: plasmoid.expanded = false
                }
            }

            // 分隔线
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Qt.rgba(1, 1, 1, 0.06)
            }

            // ── 传感器详情模式:全屏 LineChartControl ──
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: root.displayStyle === "sensor"

                LineChart {
                    id: sensorChart
                    anchors.fill: parent
                    lineColor: root.themeColors.highlight
                    fillOpacity: 0.25
                    lineWidth: 2
                    values: {
                        const arr = [];
                        for (let i = 0; i < root.history.length; ++i)
                            arr.push(root.history[i].worstPercent);
                        return arr;
                    }
                }

                PlasmaComponents.Label {
                    visible: root.history.length === 0
                    anchors.centerIn: parent
                    text: i18n("暂无历史数据,等待下次刷新")
                    color: PlasmaCore.Theme.TextColor
                    opacity: 0.6
                }
            }

            // ── 饼状/柱状模式:供应商列表 ──
            ListView {
                id: listView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                visible: root.displayStyle !== "sensor"
                model: root.providers
                spacing: 4

                delegate: Item {
                    required property var modelData
                    property int idx: index
                    width: ListView.view.width
                    height: column.implicitHeight + 4

                    Column {
                        id: column
                        width: parent.width
                        spacing: 2
                        topPadding: 4
                        leftPadding: 4
                        rightPadding: 4

                        // 供应商标题
                        Row {
                            width: parent.width
                            spacing: 4
                            Rectangle {
                                width: 6; height: 6; radius: 3
                                anchors.verticalCenter: parent.verticalCenter
                                color: {
                                    switch (modelData.ledClass) {
                                    case "led-green": return root.themeColors.positive;
                                    case "led-yellow": return root.themeColors.highlight;
                                    case "led-red": return root.themeColors.negative;
                                    default: return root.themeColors.neutral;
                                    }
                                }
                            }
                            PlasmaComponents.Label {
                                text: MockData.stripProviderSuffix(modelData.providerName)
                                color: PlasmaCore.Theme.TextColor
                                font.bold: true
                                font.pixelSize: PlasmaCore.Units.gridUnit * 0.7
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            PlasmaComponents.Label {
                                visible: modelData.sourceLabel.length > 0
                                text: modelData.sourceLabel
                                color: PlasmaCore.Theme.TextColor
                                opacity: 0.7
                                font.pixelSize: PlasmaCore.Units.gridUnit * 0.5
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            PlasmaComponents.Label {
                                visible: modelData.statusLabel.length > 0
                                text: modelData.statusLabel
                                color: {
                                    switch (modelData.ledClass) {
                                    case "led-green": return root.themeColors.positive;
                                    case "led-yellow": return root.themeColors.highlight;
                                    case "led-red": return root.themeColors.negative;
                                    default: return root.themeColors.neutral;
                                    }
                                }
                                font.pixelSize: PlasmaCore.Units.gridUnit * 0.5
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        // 错误信息
                        PlasmaComponents.Label {
                            visible: modelData.errorText.length > 0
                            text: modelData.errorText
                            color: root.themeColors.negative
                            width: parent.width
                            wrapMode: Text.Wrap
                            font.pixelSize: PlasmaCore.Units.gridUnit * 0.55
                        }

                        // PlanBar 区域
                        Column {
                            width: parent.width
                            spacing: 1
                            visible: modelData.plans.length > 0

                            Repeater {
                                model: modelData.plans

                                delegate: Item {
                                    required property var modelData
                                    width: parent.width
                                    height: planRow.implicitHeight + 2

                                    Row {
                                        id: planRow
                                        width: parent.width
                                        spacing: 4
                                        leftPadding: 8

                                        PlasmaComponents.Label {
                                            text: modelData.planName
                                            color: PlasmaCore.Theme.TextColor
                                            opacity: 0.7
                                            width: 60
                                            elide: Text.ElideRight
                                            font.pixelSize: PlasmaCore.Units.gridUnit * 0.55
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        // PlanBar 进度条:页面样式固定,所有 displayStyle 下都显示(用户要求不改)
                                        Item {
                                            width: parent.width - 220
                                            height: 6
                                            anchors.verticalCenter: parent.verticalCenter

                                            Rectangle {
                                                anchors.fill: parent; radius: 3
                                                color: Qt.rgba(PlasmaCore.Theme.backgroundColor.r,
                                                               PlasmaCore.Theme.backgroundColor.g,
                                                               PlasmaCore.Theme.backgroundColor.b, 0.4)
                                            }
                                            Rectangle {
                                                anchors.left: parent.left; anchors.top: parent.top; anchors.bottom: parent.bottom
                                                width: parent.width * (modelData.usedPercent / 100)
                                                radius: 3
                                                color: root.barColor(modelData.usedPercent)
                                                Behavior on width {
                                                    NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                                                }
                                            }
                                        }

                                        PlasmaComponents.Label {
                                            text: modelData.usedPercentLabel
                                            color: root.barColor(modelData.usedPercent)
                                            font.bold: true
                                            font.pixelSize: PlasmaCore.Units.gridUnit * 0.55
                                            width: 40
                                            horizontalAlignment: Text.AlignRight
                                            anchors.verticalCenter: parent.verticalCenter
                                        }

                                        PlasmaComponents.Label {
                                            text: modelData.resetText
                                            color: PlasmaCore.Theme.TextColor
                                            opacity: 0.6
                                            font.pixelSize: PlasmaCore.Units.gridUnit * 0.5
                                            visible: modelData.resetText.length > 0
                                            width: 80
                                            elide: Text.ElideRight
                                            horizontalAlignment: Text.AlignRight
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}