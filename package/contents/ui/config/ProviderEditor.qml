// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../../js/providerConfig.js" as ProviderConfig

Item {
    id: root

    objectName: "providerEditor"
    implicitWidth: Kirigami.Units.gridUnit * 28
    implicitHeight: form.implicitHeight

    property var candidate: ({})
    property var siblings: []
    property bool editingExisting: false
    readonly property var validation: ProviderConfig.validateProvider(candidate, siblings)
    readonly property string previewText: ProviderConfig.previewTemplate(candidate.template || "")

    function copy(value) {
        return JSON.parse(JSON.stringify(value))
    }

    function setCandidate(value, existing) {
        candidate = copy(value)
        editingExisting = existing === true
    }

    function currentCandidate() {
        return copy(candidate)
    }

    function updateField(name, value) {
        const next = copy(candidate)
        next[name] = value
        candidate = next
    }

    function updatePlan(index, name, value) {
        const next = copy(candidate)
        next.plans[index][name] = value
        candidate = next
    }

    function addPlan() {
        const next = copy(candidate)
        let number = next.plans.length + 1
        let id = "plan-" + number
        while (next.plans.some(function(plan) { return plan.id === id })) {
            ++number
            id = "plan-" + number
        }
        next.plans.push({ id: id, planName: "", unit: "" })
        candidate = next
    }

    function removePlan(index) {
        const next = copy(candidate)
        next.plans.splice(index, 1)
        candidate = next
    }

    Kirigami.FormLayout {
        id: form

        anchors.left: parent.left
        anchors.right: parent.right

        QQC2.TextField {
            objectName: "providerIdField"
            Kirigami.FormData.label: qsTr("Provider ID:")
            text: root.candidate.id || ""
            readOnly: root.editingExisting
            onTextEdited: root.updateField("id", text)
        }

        QQC2.TextField {
            Kirigami.FormData.label: qsTr("Name:")
            text: root.candidate.providerName || ""
            onTextEdited: root.updateField("providerName", text)
        }

        QQC2.ComboBox {
            Kirigami.FormData.label: qsTr("Source:")
            model: [qsTr("自定义"), qsTr("套餐"), qsTr("订阅")]
            currentIndex: Math.max(0, model.indexOf(root.candidate.sourceLabel || "自定义"))
            onActivated: root.updateField("sourceLabel", currentText)
        }

        QQC2.ComboBox {
            Kirigami.FormData.label: qsTr("Trust mode:")
            model: ["strict", "lan", "custom"]
            currentIndex: Math.max(0, model.indexOf(root.candidate.trustMode || "strict"))
            onActivated: root.updateField("trustMode", currentText)
        }

        ColumnLayout {
            Kirigami.FormData.label: qsTr("Plans:")
            Layout.fillWidth: true

            Repeater {
                model: root.candidate.plans || []

                delegate: RowLayout {
                    required property int index
                    required property var modelData

                    Layout.fillWidth: true

                    QQC2.TextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("Plan ID")
                        text: parent.modelData.id || ""
                        onTextEdited: root.updatePlan(parent.index, "id", text)
                    }

                    QQC2.TextField {
                        Layout.fillWidth: true
                        placeholderText: qsTr("Plan name")
                        text: parent.modelData.planName || ""
                        onTextEdited: root.updatePlan(parent.index, "planName", text)
                    }

                    QQC2.TextField {
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 4
                        placeholderText: qsTr("Unit")
                        text: parent.modelData.unit || ""
                        onTextEdited: root.updatePlan(parent.index, "unit", text)
                    }

                    QQC2.ToolButton {
                        icon.name: "edit-delete"
                        enabled: (root.candidate.plans || []).length > 1
                        Accessible.name: qsTr("Delete plan")
                        QQC2.ToolTip.text: Accessible.name
                        QQC2.ToolTip.visible: hovered
                        onClicked: root.removePlan(parent.index)
                    }
                }
            }

            QQC2.Button {
                text: qsTr("Add plan")
                icon.name: "list-add"
                onClicked: root.addPlan()
            }
        }

        ColumnLayout {
            Kirigami.FormData.label: qsTr("Template:")
            Layout.fillWidth: true

            QQC2.TextField {
                Layout.fillWidth: true
                text: root.candidate.template || ""
                onTextEdited: root.updateField("template", text)
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: root.previewText
                wrapMode: Text.Wrap
            }
        }

        QQC2.TextField {
            Kirigami.FormData.label: qsTr("Script:")
            text: qsTr("脚本编辑将在后续版本实现")
            readOnly: true
        }

        Kirigami.InlineMessage {
            visible: !root.validation.valid
            text: root.validation.message
            type: Kirigami.MessageType.Error
        }
    }
}
