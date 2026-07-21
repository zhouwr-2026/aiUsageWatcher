// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import "../../js/providerConfig.js" as ProviderConfig

KCM.SimpleKCM {
    id: root

    property string cfg_providers: ""
    readonly property int workingCount: providersModel.count
    property string editingId: ""
    property string pendingDeleteId: ""

    function copy(value) {
        return JSON.parse(JSON.stringify(value))
    }

    function definitionRow(definition) {
        const names = definition.plans.map(function(plan) { return plan.planName })
        return {
            providerId: definition.id,
            providerName: definition.providerName,
            planSummary: names.join(", "),
            definitionJson: JSON.stringify(definition)
        }
    }

    function definitions() {
        const items = []
        for (let i = 0; i < providersModel.count; ++i)
            items.push(JSON.parse(providersModel.get(i).definitionJson))
        return items
    }

    function indexForId(id) {
        for (let i = 0; i < providersModel.count; ++i) {
            if (providersModel.get(i).providerId === id)
                return i
        }
        return -1
    }

    function siblingsExcept(id) {
        return definitions().filter(function(definition) { return definition.id !== id })
    }

    function syncWorkingValue() {
        cfg_providers = ProviderConfig.serializeDefinitions(definitions())
    }

    function addProvider(candidate) {
        const result = ProviderConfig.validateProvider(candidate, definitions())
        if (!result.valid)
            return false
        providersModel.append(definitionRow(copy(candidate)))
        syncWorkingValue()
        return true
    }

    function updateProvider(candidate) {
        const index = indexForId(candidate.id)
        const result = ProviderConfig.validateProvider(candidate, siblingsExcept(candidate.id))
        if (index < 0 || !result.valid)
            return false
        providersModel.set(index, definitionRow(copy(candidate)))
        syncWorkingValue()
        return true
    }

    function deleteProvider(id) {
        const index = indexForId(id)
        if (index < 0)
            return false
        providersModel.remove(index)
        syncWorkingValue()
        return true
    }

    function nextProviderId() {
        let number = providersModel.count + 1
        let id = "provider-" + number
        while (indexForId(id) >= 0) {
            ++number
            id = "provider-" + number
        }
        return id
    }

    function beginAdd() {
        editingId = ""
        providerEditor.siblings = definitions()
        providerEditor.setCandidate({
            id: nextProviderId(),
            providerName: "",
            sourceLabel: "自定义",
            trustMode: "strict",
            template: "%1 限额  %2/%3  重置于 %4",
            plans: [{ id: "plan-1", planName: "", unit: "" }]
        }, false)
        providerDialog.mode = "edit"
        providerDialog.title = qsTr("Add provider")
        providerDialog.open()
    }

    function beginEdit(id) {
        const index = indexForId(id)
        if (index < 0)
            return false
        editingId = id
        providerEditor.siblings = siblingsExcept(id)
        providerEditor.setCandidate(
            JSON.parse(providersModel.get(index).definitionJson), true)
        providerDialog.mode = "edit"
        providerDialog.title = qsTr("Edit provider")
        providerDialog.open()
        return true
    }

    function beginDelete(id) {
        if (indexForId(id) < 0)
            return false
        pendingDeleteId = id
        providerDialog.mode = "delete"
        providerDialog.title = qsTr("Delete provider")
        providerDialog.open()
        return true
    }

    function cancelEditor() {
        providerDialog.reject()
    }

    function acceptDialog() {
        if (providerDialog.mode === "delete") {
            deleteProvider(pendingDeleteId)
            return
        }
        const candidate = providerEditor.currentCandidate()
        if (editingId)
            updateProvider(candidate)
        else
            addProvider(candidate)
    }

    Component.onCompleted: {
        const parsed = ProviderConfig.parseWorkingDefinitions(cfg_providers)
        for (let i = 0; i < parsed.length; ++i)
            providersModel.append(definitionRow(parsed[i]))
    }

    ListModel {
        id: providersModel
    }

    ColumnLayout {
        width: parent.width

        RowLayout {
            Layout.fillWidth: true

            QQC2.Label {
                Layout.fillWidth: true
                text: qsTr("Providers")
                font.bold: true
            }

            QQC2.Button {
                text: qsTr("Add")
                icon.name: "list-add"
                onClicked: root.beginAdd()
            }
        }

        Repeater {
            model: providersModel

            delegate: QQC2.ItemDelegate {
                id: providerDelegate

                required property string providerId
                required property string providerName
                required property string planSummary

                Layout.fillWidth: true
                contentItem: RowLayout {
                    ColumnLayout {
                        Layout.fillWidth: true

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: providerDelegate.providerName
                            elide: Text.ElideRight
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: providerDelegate.planSummary
                            color: Kirigami.Theme.disabledTextColor
                            elide: Text.ElideRight
                        }
                    }

                    QQC2.ToolButton {
                        icon.name: "document-edit"
                        Accessible.name: qsTr("Edit %1").arg(providerDelegate.providerName)
                        QQC2.ToolTip.text: Accessible.name
                        QQC2.ToolTip.visible: hovered
                        onClicked: root.beginEdit(providerDelegate.providerId)
                    }

                    QQC2.ToolButton {
                        icon.name: "edit-delete"
                        Accessible.name: qsTr("Delete %1").arg(providerDelegate.providerName)
                        QQC2.ToolTip.text: Accessible.name
                        QQC2.ToolTip.visible: hovered
                        onClicked: root.beginDelete(providerDelegate.providerId)
                    }
                }
            }
        }
    }

    QQC2.Dialog {
        id: providerDialog

        objectName: "providerDialog"
        property string mode: "edit"

        parent: root
        anchors.centerIn: parent
        modal: true
        standardButtons: QQC2.Dialog.Save | QQC2.Dialog.Cancel
        width: Math.min(root.width - Kirigami.Units.largeSpacing * 2,
                        Kirigami.Units.gridUnit * 32)

        function refreshAcceptButton() {
            const button = standardButton(QQC2.Dialog.Save)
            if (button)
                button.enabled = mode === "delete" || providerEditor.validation.valid
        }

        onOpened: refreshAcceptButton()
        onAccepted: root.acceptDialog()

        Connections {
            target: providerEditor
            function onValidationChanged() {
                providerDialog.refreshAcceptButton()
            }
        }

        contentItem: ColumnLayout {
            ProviderEditor {
                id: providerEditor

                visible: providerDialog.mode === "edit"
                Layout.fillWidth: true
            }

            QQC2.Label {
                visible: providerDialog.mode === "delete"
                text: qsTr("Delete this provider? This action cannot be undone.")
                wrapMode: Text.Wrap
            }
        }
    }
}
