// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid
import "../../js/providerConfig.js" as ProviderConfig

KCM.SimpleKCM {
    id: root

    property string cfg_providers: ""
    property string cfg_providersDefault: ""
    property string cfg_compactStyle: "pie"
    property string cfg_compactStyleDefault: "pie"
    property int cfg_refreshIntervalSec: 60
    property int cfg_refreshIntervalSecDefault: 60
    property int cfg_opacityPercent: 80
    property int cfg_opacityPercentDefault: 80
    property bool cfg_keepPanelOpen: false
    property bool cfg_keepPanelOpenDefault: false
    readonly property int workingCount: providersModel.count
    readonly property var usageBackend: Plasmoid
    property string editingId: ""
    property string pendingDeleteId: ""
    property bool editorVisible: false
    property bool miniMaxCredentialConfigured: false
    property bool miniMaxCredentialBusy: false
    property bool miniMaxCredentialError: false
    property string miniMaxCredentialStatus: qsTr("尚未保存 API Key")

    function copy(value) {
        return JSON.parse(JSON.stringify(value))
    }

    function definitionRow(definition) {
        const names = definition.plans.map(function(plan) { return plan.planName })
        return {
            providerId: definition.id,
            providerName: definition.providerName,
            planSummary: names.join("、"),
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
        editorVisible = true
    }

    function beginEdit(id) {
        const index = indexForId(id)
        if (index < 0)
            return false
        editingId = id
        providerEditor.siblings = siblingsExcept(id)
        providerEditor.setCandidate(
            JSON.parse(providersModel.get(index).definitionJson), true)
        editorVisible = true
        return true
    }

    function beginDelete(id) {
        if (indexForId(id) < 0)
            return false
        pendingDeleteId = id
        deleteDialog.open()
        return true
    }

    function cancelEditor() {
        editorVisible = false
        editingId = ""
    }

    function saveEditor() {
        const candidate = providerEditor.currentCandidate()
        const saved = editingId ? updateProvider(candidate) : addProvider(candidate)
        if (saved)
            cancelEditor()
        return saved
    }

    function syncCredentialState() {
        const configured = usageBackend["miniMaxCredentialConfigured"]
        const busy = usageBackend["miniMaxCredentialBusy"]
        const error = usageBackend["miniMaxCredentialError"]
        const status = usageBackend["miniMaxCredentialStatus"]
        miniMaxCredentialConfigured = configured === true
        miniMaxCredentialBusy = busy === true
        miniMaxCredentialError = error === true
        miniMaxCredentialStatus = typeof status === "string" && status.length > 0
            ? status : qsTr("尚未保存 API Key")
    }

    function saveMiniMaxApiKey(apiKey) {
        const saveFunction = usageBackend["saveMiniMaxApiKey"]
        if (typeof saveFunction !== "function") {
            miniMaxCredentialError = true
            miniMaxCredentialStatus = qsTr("原生凭据后端未加载，请重启 Plasma 后重试")
            return false
        }
        saveFunction.call(usageBackend, apiKey)
        return true
    }

    function clearMiniMaxApiKey() {
        const clearFunction = usageBackend["clearMiniMaxApiKey"]
        if (typeof clearFunction !== "function") {
            miniMaxCredentialError = true
            miniMaxCredentialStatus = qsTr("原生凭据后端未加载，请重启 Plasma 后重试")
            return false
        }
        clearFunction.call(usageBackend)
        return true
    }

    Component.onCompleted: {
        const parsed = ProviderConfig.parseWorkingDefinitions(cfg_providers)
        for (let i = 0; i < parsed.length; ++i)
            providersModel.append(definitionRow(parsed[i]))
        syncCredentialState()
    }

    Connections {
        target: root.usageBackend
        ignoreUnknownSignals: true

        function onMiniMaxCredentialConfiguredChanged() { root.syncCredentialState() }
        function onMiniMaxCredentialStatusChanged() { root.syncCredentialState() }
        function onMiniMaxCredentialBusyChanged() { root.syncCredentialState() }
        function onMiniMaxCredentialErrorChanged() { root.syncCredentialState() }
    }

    ListModel {
        id: providersModel
    }

    header: QQC2.ToolBar {
        visible: root.editorVisible

        contentItem: RowLayout {
            QQC2.ToolButton {
                icon.name: "go-previous"
                text: qsTr("返回")
                display: QQC2.AbstractButton.TextBesideIcon
                onClicked: root.cancelEditor()
            }

            Kirigami.Heading {
                Layout.fillWidth: true
                level: 2
                text: root.editingId
                    ? qsTr("编辑 %1").arg(providerEditor.candidate.providerName || qsTr("供应商"))
                    : qsTr("添加供应商")
                elide: Text.ElideRight
            }
        }
    }

    StackLayout {
        width: parent.width
        currentIndex: root.editorVisible ? 1 : 0

        ColumnLayout {
            id: providerListPage

            objectName: "providerListPage"
            Layout.fillWidth: true

            RowLayout {
                Layout.fillWidth: true

                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 2
                    text: qsTr("供应商")
                }

                QQC2.Button {
                    text: qsTr("添加供应商")
                    icon.name: "list-add"
                    onClicked: root.beginAdd()
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: qsTr("选择供应商以编辑套餐和查询凭据。")
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.Wrap
            }

            Repeater {
                model: providersModel

                delegate: QQC2.ItemDelegate {
                    id: providerDelegate

                    required property string providerId
                    required property string providerName
                    required property string planSummary

                    Layout.fillWidth: true
                    onClicked: root.beginEdit(providerDelegate.providerId)

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
                            icon.name: "edit-delete"
                            Accessible.name: qsTr("删除 %1").arg(providerDelegate.providerName)
                            QQC2.ToolTip.text: Accessible.name
                            QQC2.ToolTip.visible: hovered
                            onClicked: root.beginDelete(providerDelegate.providerId)
                        }

                        Kirigami.Icon {
                            source: "go-next"
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: width
                        }
                    }
                }
            }
        }

        ColumnLayout {
            id: editorPage

            objectName: "providerEditorPage"
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            ProviderEditor {
                id: providerEditor

                Layout.fillWidth: true
                credentialConfigured: root.miniMaxCredentialConfigured
                credentialBusy: root.miniMaxCredentialBusy
                credentialError: root.miniMaxCredentialError
                credentialStatus: root.miniMaxCredentialStatus
                onSaveApiKeyRequested: apiKey => root.saveMiniMaxApiKey(apiKey)
                onClearApiKeyRequested: root.clearMiniMaxApiKey()
            }

            RowLayout {
                Layout.fillWidth: true

                Item {
                    Layout.fillWidth: true
                }

                QQC2.Button {
                    text: qsTr("取消")
                    icon.name: "dialog-cancel"
                    onClicked: root.cancelEditor()
                }

                QQC2.Button {
                    objectName: "saveProviderButton"
                    text: qsTr("完成")
                    icon.name: "dialog-ok-apply"
                    enabled: providerEditor.validation.valid
                    highlighted: true
                    onClicked: root.saveEditor()
                }
            }
        }
    }

    Kirigami.PromptDialog {
        id: deleteDialog

        objectName: "deleteProviderDialog"
        parent: root
        title: qsTr("删除供应商？")
        subtitle: qsTr("将从当前配置中移除该供应商及其套餐。")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: root.deleteProvider(root.pendingDeleteId)
    }
}
