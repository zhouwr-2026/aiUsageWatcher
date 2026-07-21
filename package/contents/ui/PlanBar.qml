import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents

Item {
    id: root

    property string planName: ""
    property real usedPercent: -1
    property string usedPercentLabel: "—"
    property string barClass: "bar-gray"
    property string usedText: ""
    property string totalText: ""
    property string unitText: ""
    property string unitOverflow: ""
    property string resetText: ""
    property string extraText: ""
    property string templateText: "%1 限额  %2/%3  重置于 %4"

    readonly property string renderedTemplate: renderTemplate(templateText, [
        planName, usedText, totalText, resetText
    ])

    function usageColor(usageClass) {
        switch (usageClass) {
        case "bar-green": return Kirigami.Theme.positiveTextColor
        case "bar-yellow": return Kirigami.Theme.neutralTextColor
        case "bar-red": return Kirigami.Theme.negativeTextColor
        default: return Kirigami.Theme.disabledTextColor
        }
    }

    function renderTemplate(template, values) {
        let rendered = template || "%1 限额  %2/%3  重置于 %4"
        for (let i = 0; i < values.length; ++i) {
            const value = values[i] === undefined || values[i] === null ? "" : String(values[i])
            rendered = rendered.split("%" + (i + 1)).join(value)
        }
        return rendered
    }

    implicitHeight: content.implicitHeight
    implicitWidth: Kirigami.Units.gridUnit * 16

    ColumnLayout {
        id: content

        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents.Label {
                Layout.preferredWidth: Kirigami.Units.gridUnit * 5
                Layout.maximumWidth: Kirigami.Units.gridUnit * 7
                Layout.minimumWidth: 0
                text: root.planName
                color: Kirigami.Theme.textColor
                elide: Text.ElideRight
            }

            Item {
                Layout.fillWidth: true
                Layout.minimumWidth: Kirigami.Units.gridUnit * 2
                Layout.preferredHeight: Kirigami.Units.smallSpacing * 2

                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: Kirigami.Theme.backgroundColor
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: root.usedPercent < 0 ? 0 : Math.max(
                               Kirigami.Units.smallSpacing,
                               parent.width * Math.max(0, Math.min(100, root.usedPercent)) / 100)
                    radius: height / 2
                    color: root.usageColor(root.barClass)

                    Behavior on width {
                        NumberAnimation {
                            duration: 300
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }

            PlasmaComponents.Label {
                Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                text: root.usedPercentLabel
                color: root.usageColor(root.barClass)
                font.bold: true
                horizontalAlignment: Text.AlignRight
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            PlasmaComponents.Label {
                objectName: "templateTextLabel"

                Layout.fillWidth: true
                Layout.minimumWidth: 0
                text: root.renderedTemplate
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight
            }

            PlasmaComponents.Label {
                visible: text.length > 0
                Layout.maximumWidth: root.width / 3
                text: [root.unitText, root.unitOverflow, root.extraText]
                        .filter(value => value.length > 0).join(" · ")
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                elide: Text.ElideRight
            }
        }
    }
}
