import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.SimpleKCM {
    id: root

    property string cfg_compactStyle: "pie"
    property string cfg_compactStyleDefault: "pie"
    property string cfg_panelStyle: "bar"
    property string cfg_panelStyleDefault: "bar"
    property string cfg_displayStrategy: "polling"
    property string cfg_displayStrategyDefault: "polling"
    property alias cfg_pollingIntervalSec: pollingInterval.value
    property int cfg_pollingIntervalSecDefault: 5
    property string cfg_eventMode: "dbus"
    property string cfg_eventModeDefault: "dbus"
    property alias cfg_highlightDurationSec: highlightDuration.value
    property int cfg_highlightDurationSecDefault: 30
    property alias cfg_refreshIntervalSec: refreshInterval.value
    property int cfg_refreshIntervalSecDefault: 60
    property alias cfg_opacityPercent: opacity.value
    property int cfg_opacityPercentDefault: 80
    property alias cfg_keepPanelOpen: keepPanelOpen.checked
    property bool cfg_keepPanelOpenDefault: false
    property alias cfg_sortMode: sortModeControl.currentValue
    property string cfg_sortModeDefault: "default"
    property string cfg_customOrder: ""
    property string cfg_customOrderDefault: ""
    property string cfg_providers: ""
    property string cfg_providersDefault: ""

    readonly property var sortModeOptions: [
        { text: qsTr("默认顺序"), value: "default" },
        { text: qsTr("字母 A-Z"), value: "alphabetical" },
        { text: qsTr("已用% 降序"), value: "usedPercent" },
        { text: qsTr("剩余% 降序"), value: "remainingPercent" },
        { text: qsTr("最近重置"), value: "nextReset" },
        { text: qsTr("自定义顺序"), value: "custom" }
    ]

    Kirigami.FormLayout {
        QQC2.ComboBox {
            id: sortModeControl

            objectName: "sortModeControl"
            Kirigami.FormData.label: qsTr("面板供应商排序：")
            model: root.sortModeOptions
            textRole: "text"
            valueRole: "value"
            currentIndex: {
                const idx = root.sortModeOptions.findIndex(function(opt) {
                    return opt.value === (root.cfg_sortMode || "default")
                })
                return Math.max(0, idx)
            }
            onActivated: root.cfg_sortMode = currentValue
        }

        QQC2.ComboBox {
            id: compactStyle

            objectName: "compactStyleControl"
            Kirigami.FormData.label: qsTr("小图标图表：")
            model: [
                { text: qsTr("环形饼图"), value: "pie" },
                { text: qsTr("水平进度条"), value: "bar" }
            ]
            textRole: "text"
            valueRole: "value"
            currentIndex: root.cfg_compactStyle === "bar" ? 1 : 0
            onActivated: root.cfg_compactStyle = currentValue
        }

        QQC2.SpinBox {
            id: refreshInterval

            objectName: "refreshIntervalControl"
            Kirigami.FormData.label: qsTr("数据刷新间隔（秒）：")
            from: 10
            to: 3600
            value: 60
            editable: true
        }

        QQC2.ComboBox {
            id: panelStyle

            objectName: "panelStyleControl"
            Kirigami.FormData.label: qsTr("面板图表：")
            model: [
                { text: qsTr("水平柱状图"), value: "bar" },
                { text: qsTr("环形饼图"), value: "pie" }
            ]
            textRole: "text"
            valueRole: "value"
            currentIndex: root.cfg_panelStyle === "pie" ? 1 : 0
            onActivated: root.cfg_panelStyle = currentValue
        }

        QQC2.ComboBox {
            id: displayStrategy

            objectName: "displayStrategyControl"
            Kirigami.FormData.label: qsTr("多模型显示：")
            model: [
                { text: qsTr("定时轮询"), value: "polling" },
                { text: qsTr("D-Bus 事件优先"), value: "event" }
            ]
            textRole: "text"
            valueRole: "value"
            currentIndex: root.cfg_displayStrategy === "event" ? 1 : 0
            onActivated: root.cfg_displayStrategy = currentValue
        }

        QQC2.SpinBox {
            id: pollingInterval

            objectName: "pollingIntervalControl"
            Kirigami.FormData.label: qsTr("轮询间隔（秒）：")
            from: 1
            to: 300
            value: 5
            editable: true
        }

        QQC2.SpinBox {
            id: highlightDuration

            objectName: "highlightDurationControl"
            Kirigami.FormData.label: qsTr("事件高亮（秒）：")
            from: 1
            to: 600
            value: 30
            editable: true
            enabled: root.cfg_displayStrategy === "event"
        }

        Kirigami.InlineMessage {
            Kirigami.FormData.label: qsTr("事件接口：")
            Layout.fillWidth: true
            visible: root.cfg_displayStrategy === "event"
            text: qsTr("监听会话 D-Bus 信号 org.kde.quotaPilot.ModelActivated；HTTP 回调将在安全隔离后提供。")
            type: Kirigami.MessageType.Information
        }

        QQC2.Slider {
            id: opacity

            objectName: "opacityControl"
            Kirigami.FormData.label: qsTr("面板不透明度：%1%").arg(Math.round(value))
            from: 20
            to: 100
            value: 80
            stepSize: 1
            snapMode: QQC2.Slider.SnapAlways
        }

        QQC2.CheckBox {
            id: keepPanelOpen

            objectName: "keepPanelOpenControl"
            text: qsTr("保持面板打开")
            checked: false
        }
    }
}
