import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami

KCM.SimpleKCM {
    id: root

    property string cfg_providers: ""
    property string cfg_providersDefault: ""
    property string cfg_compactStyle: "pie"
    property string cfg_compactStyleDefault: "pie"
    property alias cfg_refreshIntervalSec: refreshInterval.value
    property int cfg_refreshIntervalSecDefault: 60
    property alias cfg_opacityPercent: opacity.value
    property int cfg_opacityPercentDefault: 80
    property alias cfg_keepPanelOpen: keepPanelOpen.checked
    property bool cfg_keepPanelOpenDefault: false

    Kirigami.FormLayout {
        QQC2.ComboBox {
            id: compactStyle

            objectName: "compactStyleControl"
            Kirigami.FormData.label: qsTr("Compact style:")
            model: [
                { text: qsTr("Pie chart"), value: "pie" },
                { text: qsTr("Bar"), value: "bar" }
            ]
            textRole: "text"
            valueRole: "value"
            currentIndex: root.cfg_compactStyle === "bar" ? 1 : 0
            onActivated: root.cfg_compactStyle = currentValue
        }

        QQC2.SpinBox {
            id: refreshInterval

            objectName: "refreshIntervalControl"
            Kirigami.FormData.label: qsTr("Refresh interval (seconds):")
            from: 10
            to: 3600
            value: 60
            editable: true
        }

        QQC2.Slider {
            id: opacity

            objectName: "opacityControl"
            Kirigami.FormData.label: qsTr("Panel opacity: %1%").arg(Math.round(value))
            from: 20
            to: 100
            value: 80
            stepSize: 1
            snapMode: QQC2.Slider.SnapAlways
        }

        QQC2.CheckBox {
            id: keepPanelOpen

            objectName: "keepPanelOpenControl"
            text: qsTr("Keep panel open")
            checked: false
        }
    }
}
