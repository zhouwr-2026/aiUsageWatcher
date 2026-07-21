// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../../js/providerConfig.js" as ProviderConfig

Item {
    id: root

    objectName: "providerEditor"
    implicitHeight: form.implicitHeight

    property var candidate: ({})
    property var siblings: []
    property bool editingExisting: false
    property bool credentialConfigured: false
    property bool credentialBusy: false
    property bool credentialError: false
    property string credentialStatus: qsTr("尚未保存 API Key")
    readonly property bool isMiniMax: (candidate.id || "").toLowerCase() === "minimax"
    readonly property var validation: ProviderConfig.validateProvider(candidate, siblings)
    readonly property string previewText: ProviderConfig.previewTemplate(candidate.template || "")

    signal saveApiKeyRequested(string apiKey)
    signal clearApiKeyRequested()

    function copy(value) {
        return JSON.parse(JSON.stringify(value))
    }

    function setCandidate(value, existing) {
        candidate = copy(value)
        editingExisting = existing === true
        apiKeyField.clear()
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

        Kirigami.Separator {
            Kirigami.FormData.label: qsTr("基本信息")
            Kirigami.FormData.isSection: true
        }

        QQC2.TextField {
            objectName: "providerIdField"
            Kirigami.FormData.label: qsTr("供应商 ID：")
            text: root.candidate.id || ""
            readOnly: root.editingExisting
            onTextEdited: root.updateField("id", text)
        }

        QQC2.TextField {
            Kirigami.FormData.label: qsTr("显示名称：")
            text: root.candidate.providerName || ""
            onTextEdited: root.updateField("providerName", text)
        }

        QQC2.ComboBox {
            Kirigami.FormData.label: qsTr("数据来源：")
            model: [qsTr("自定义"), qsTr("套餐"), qsTr("订阅")]
            currentIndex: Math.max(0, model.indexOf(root.candidate.sourceLabel || "自定义"))
            onActivated: root.updateField("sourceLabel", currentText)
        }

        QQC2.ComboBox {
            Kirigami.FormData.label: qsTr("脚本信任模式：")
            model: ["strict", "lan", "custom"]
            currentIndex: Math.max(0, model.indexOf(root.candidate.trustMode || "strict"))
            onActivated: root.updateField("trustMode", currentText)
        }

        Kirigami.Separator {
            visible: root.isMiniMax
            Kirigami.FormData.label: qsTr("MiniMax API 凭据")
            Kirigami.FormData.isSection: true
        }

        QQC2.TextField {
            id: apiKeyField

            objectName: "miniMaxApiKeyField"
            visible: root.isMiniMax
            Kirigami.FormData.label: qsTr("API Key：")
            placeholderText: root.credentialConfigured
                ? qsTr("输入新 Key 以替换已保存凭据")
                : qsTr("请输入 MiniMax API Key")
            echoMode: TextInput.Password
            passwordCharacter: "●"
            enabled: !root.credentialBusy
            inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoPredictiveText

            Keys.onReturnPressed: {
                if (text.trim().length === 0 || root.credentialBusy)
                    return
                const apiKey = text
                clear()
                root.saveApiKeyRequested(apiKey)
            }
        }

        RowLayout {
            visible: root.isMiniMax
            Layout.fillWidth: true

            QQC2.Button {
                objectName: "saveMiniMaxApiKeyButton"
                text: root.credentialConfigured ? qsTr("更新 API Key") : qsTr("保存 API Key")
                icon.name: "document-save"
                enabled: apiKeyField.text.trim().length > 0 && !root.credentialBusy
                onClicked: {
                    const apiKey = apiKeyField.text
                    apiKeyField.clear()
                    root.saveApiKeyRequested(apiKey)
                }
            }

            QQC2.Button {
                text: qsTr("移除已保存 Key")
                icon.name: "edit-delete"
                enabled: root.credentialConfigured && !root.credentialBusy
                onClicked: root.clearApiKeyRequested()
            }

            QQC2.BusyIndicator {
                visible: root.credentialBusy
                running: visible
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: width
            }
        }

        Kirigami.InlineMessage {
            objectName: "miniMaxCredentialMessage"
            visible: root.isMiniMax
            Layout.fillWidth: true
            text: root.credentialStatus + "\n" + qsTr("凭据由 KDE 钱包安全保存，不会写入小组件配置。")
            type: root.credentialError
                ? Kirigami.MessageType.Error
                : (root.credentialConfigured
                   ? Kirigami.MessageType.Positive
                   : Kirigami.MessageType.Information)
        }

        Kirigami.Separator {
            Kirigami.FormData.label: qsTr("套餐限额")
            Kirigami.FormData.isSection: true
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            Repeater {
                model: root.candidate.plans || []

                delegate: ColumnLayout {
                    id: planColumn

                    required property int index
                    required property var modelData

                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.fillWidth: true

                        Kirigami.Heading {
                            Layout.fillWidth: true
                            level: 4
                            text: qsTr("套餐 %1").arg(planColumn.index + 1)
                        }

                        QQC2.ToolButton {
                            icon.name: "edit-delete"
                            enabled: (root.candidate.plans || []).length > 1
                            Accessible.name: qsTr("删除套餐 %1").arg(planColumn.index + 1)
                            QQC2.ToolTip.text: Accessible.name
                            QQC2.ToolTip.visible: hovered
                            onClicked: root.removePlan(planColumn.index)
                        }
                    }

                    Kirigami.FormLayout {
                        Layout.fillWidth: true

                        QQC2.TextField {
                            Kirigami.FormData.label: qsTr("套餐 ID：")
                            text: planColumn.modelData.id || ""
                            onTextEdited: root.updatePlan(planColumn.index, "id", text)
                        }

                        QQC2.TextField {
                            Kirigami.FormData.label: qsTr("显示名称：")
                            text: planColumn.modelData.planName || ""
                            onTextEdited: root.updatePlan(planColumn.index, "planName", text)
                        }

                        QQC2.TextField {
                            Kirigami.FormData.label: qsTr("单位：")
                            text: planColumn.modelData.unit || ""
                            onTextEdited: root.updatePlan(planColumn.index, "unit", text)
                        }
                    }
                }
            }

            QQC2.Button {
                text: qsTr("添加套餐")
                icon.name: "list-add"
                onClicked: root.addPlan()
            }
        }

        Kirigami.Separator {
            Kirigami.FormData.label: qsTr("显示模板")
            Kirigami.FormData.isSection: true
        }

        ColumnLayout {
            Layout.fillWidth: true

            QQC2.TextField {
                Layout.fillWidth: true
                text: root.candidate.template || ""
                onTextEdited: root.updateField("template", text)
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: root.previewText
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.Wrap
            }
        }

        Kirigami.InlineMessage {
            visible: !root.validation.valid
            Layout.fillWidth: true
            text: root.validation.message
            type: Kirigami.MessageType.Error
        }
    }
}
