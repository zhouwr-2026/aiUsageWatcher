// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kcmutils as KCM
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid
import "../../js/providerConfig.js" as ProviderConfig
import "../../js/providerRegistry.js" as ProviderRegistry
import "../../js/scriptTools.js" as ScriptTools

KCM.SimpleKCM {
    id: root

    property string cfg_providers: ""
    property string cfg_providersDefault: ""
    property string cfg_compactStyle: "pie"
    property string cfg_compactStyleDefault: "pie"
    property string cfg_panelStyle: "bar"
    property string cfg_panelStyleDefault: "bar"
    property string cfg_displayStrategy: "polling"
    property string cfg_displayStrategyDefault: "polling"
    property int cfg_pollingIntervalSec: 5
    property int cfg_pollingIntervalSecDefault: 5
    property string cfg_eventMode: "dbus"
    property string cfg_eventModeDefault: "dbus"
    property int cfg_highlightDurationSec: 30
    property int cfg_highlightDurationSecDefault: 30
    property int cfg_refreshIntervalSec: 60
    property int cfg_refreshIntervalSecDefault: 60
    property int cfg_opacityPercent: 80
    property int cfg_opacityPercentDefault: 80
    property bool cfg_keepPanelOpen: false
    property bool cfg_keepPanelOpenDefault: false
    property string cfg_customOrder: ""
    property string cfg_customOrderDefault: ""
    readonly property int workingCount: providersModel.count
    readonly property var usageBackend: Plasmoid
    property string editingId: ""
    property bool creatingProvider: false
    property string pendingDeleteId: ""
    property bool editorVisible: false
    property bool miniMaxCredentialConfigured: false
    property bool miniMaxCredentialBusy: false
    property bool miniMaxCredentialError: false
    property string miniMaxCredentialStatus: qsTr("尚未保存 API Key")
    property bool miniMaxUsageLoading: false
    property string miniMaxUsageStatus: qsTr("未配置")
    property string miniMaxUsageError: ""
    property bool codexLoggedIn: false
    property bool codexLoginBusy: false
    property bool codexLoginError: false
    property string codexLoginStatus: qsTr("正在检查登录状态…")
    property string codexDeviceCode: ""
    property string codexDeviceUrl: "https://auth.openai.com/codex/device"
    property var codexAccounts: []
    property bool codexUsageLoading: false
    property string codexUsageStatus: qsTr("未登录")
    property string codexUsageError: ""
    property string cfg_sortMode: Plasmoid.configuration.sortMode || "default"
    property string cfg_sortModeDefault: "default"

    function copy(value) {
        return JSON.parse(JSON.stringify(value))
    }

    function definitionRow(definition) {
        const names = definition.plans.map(function(plan) { return plan.planName })
        const enabled = typeof definition.enabled === "boolean" ? definition.enabled : true
        const catalogId = definition.catalogId || ""
        const logoPath = typeof definition.logoPath === "string" ? definition.logoPath : ""
        const isCustom = catalogId === "custom" || catalogId === ""
        const logoSource = logoPath.length > 0
            ? logoPath
            : (isCustom ? "" : "data:image/svg+xml;utf8," + ProviderRegistry.logoSvgFor(catalogId))
        const logoChar = (definition.providerName || "").trim().charAt(0).toUpperCase()
        const defCopy = JSON.parse(JSON.stringify(definition))
        defCopy.enabled = enabled
        return {
            catalogId: catalogId,
            providerId: definition.id,
            providerName: definition.providerName,
            planSummary: names.join("、"),
            enabled: enabled,
            logoSource: logoSource,
            logoChar: logoChar,
            definitionJson: JSON.stringify(defCopy)
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
        Qt.callLater(root.syncWorkingValue)
        return true
    }

    function updateProvider(candidate, originalId) {
        const existingId = originalId || candidate.id
        const index = indexForId(existingId)
        const result = ProviderConfig.validateProvider(candidate, siblingsExcept(existingId))
        if (index < 0 || !result.valid)
            return false
        providersModel.set(index, definitionRow(copy(candidate)))
        editingId = candidate.id
        Qt.callLater(root.syncWorkingValue)
        return true
    }

    function deleteProvider(id) {
        const index = indexForId(id)
        if (index < 0)
            return false
        providersModel.remove(index)
        Qt.callLater(root.syncWorkingValue)
        return true
    }

    function moveProvider(id, offset) {
        const index = indexForId(id)
        const destination = index + offset
        if (index < 0 || destination < 0 || destination >= providersModel.count)
            return false
        providersModel.move(index, destination, 1)
        Qt.callLater(root.syncWorkingValue)
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
        creatingProvider = true
        providerEditor.siblings = definitions()
        providerEditor.setCandidate({
            catalogId: "custom",
            id: nextProviderId(),
            providerName: "",
            website: "",
            vendor: "自定义",
            sourceLabel: "自定义",
            trustMode: "strict",
            template: "%1 限额  %2/%3  重置于 %4",
            script: ScriptTools.DEFAULT_SCRIPT,
            plans: [{
                id: "quota-1",
                planName: "",
                unit: "",
                sourceType: "http-js",
                usedVariable: "${used}",
                limitVariable: "${limit}"
            }]
        }, false)
        editorVisible = true
    }

    function beginEdit(id) {
        const index = indexForId(id)
        if (index < 0)
            return false
        editingId = id
        creatingProvider = false
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
        creatingProvider = false
    }

    function syncEditorCandidate() {
        if (!editorVisible)
            return false
        const candidate = providerEditor.currentCandidate()
        if (creatingProvider && !editingId) {
            if (!addProvider(candidate))
                return false
            editingId = candidate.id
            providerEditor.editingExisting = true
            return true
        }
        return updateProvider(candidate, editingId)
    }

    function saveConfig() {
        if (!editorVisible)
            return true
        const saved = syncEditorCandidate()
        if (!saved)
            console.warn("AIQuotaPilot: provider save rejected:",
                         providerEditor.validation.message)
        return saved
    }

    function syncCredentialState() {
        const configured = usageBackend["miniMaxCredentialConfigured"]
        const busy = usageBackend["miniMaxCredentialBusy"]
        const error = usageBackend["miniMaxCredentialError"]
        const status = usageBackend["miniMaxCredentialStatus"]
        const snapshot = usageBackend["miniMaxSnapshot"]
        miniMaxCredentialConfigured = configured === true
        miniMaxCredentialBusy = busy === true
        miniMaxCredentialError = error === true
        miniMaxCredentialStatus = typeof status === "string" && status.length > 0
            ? status : qsTr("尚未保存 API Key")
        miniMaxUsageLoading = usageBackend["miniMaxLoading"] === true
        miniMaxUsageStatus = snapshot && typeof snapshot.statusLabel === "string"
            ? snapshot.statusLabel : qsTr("未配置")
        miniMaxUsageError = snapshot && typeof snapshot.errorText === "string"
            ? snapshot.errorText : ""
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

    function refreshMiniMaxUsage() {
        const operation = usageBackend["refreshMiniMax"]
        if (typeof operation !== "function") {
            miniMaxUsageError = qsTr("MiniMax 查询后端未加载，请重启 Plasma 后重试")
            return false
        }
        operation.call(usageBackend)
        return true
    }

    function syncCodexLoginState() {
        codexLoggedIn = usageBackend["codexLoggedIn"] === true
        codexLoginBusy = usageBackend["codexLoginBusy"] === true
        codexLoginError = usageBackend["codexLoginError"] === true
        const status = usageBackend["codexLoginStatus"]
        const code = usageBackend["codexDeviceCode"]
        const url = usageBackend["codexDeviceUrl"]
        const accounts = usageBackend["codexAccounts"]
        const snapshot = usageBackend["codexSnapshot"]
        codexLoginStatus = typeof status === "string" && status.length > 0
            ? status : qsTr("尚未登录 Codex")
        codexDeviceCode = typeof code === "string" ? code : ""
        codexDeviceUrl = typeof url === "string" && url.length > 0
            ? url : "https://auth.openai.com/codex/device"
        codexAccounts = Array.isArray(accounts) ? accounts : []
        codexUsageLoading = usageBackend["codexUsageLoading"] === true
        codexUsageStatus = snapshot && typeof snapshot.statusLabel === "string"
            ? snapshot.statusLabel : qsTr("未登录")
        codexUsageError = snapshot && typeof snapshot.errorText === "string"
            ? snapshot.errorText : ""
    }

    function callCodexBackend(name) {
        const operation = usageBackend[name]
        if (typeof operation !== "function") {
            codexLoginError = true
            codexLoginStatus = qsTr("Codex 登录后端未加载，请重启 Plasma 后重试")
            return false
        }
        operation.call(usageBackend)
        return true
    }

    function removeCodexAccount(profileId) {
        const operation = usageBackend["removeCodexAccount"]
        if (typeof operation !== "function") {
            codexLoginError = true
            codexLoginStatus = qsTr("Codex 账号后端未加载，请重启 Plasma 后重试")
            return false
        }
        operation.call(usageBackend, profileId)
        return true
    }

    Component.onCompleted: {
        const parsed = ProviderConfig.parseWorkingDefinitions(cfg_providers)
        for (let i = 0; i < parsed.length; ++i)
            providersModel.append(definitionRow(parsed[i]))
        syncCredentialState()
        syncCodexLoginState()
    }

    Connections {
        target: root.usageBackend
        ignoreUnknownSignals: true

        function onMiniMaxCredentialConfiguredChanged() { root.syncCredentialState() }
        function onMiniMaxCredentialStatusChanged() { root.syncCredentialState() }
        function onMiniMaxCredentialBusyChanged() { root.syncCredentialState() }
        function onMiniMaxCredentialErrorChanged() { root.syncCredentialState() }
        function onMiniMaxSnapshotChanged() { root.syncCredentialState() }
        function onMiniMaxLoadingChanged() { root.syncCredentialState() }
        function onCodexLoggedInChanged() { root.syncCodexLoginState() }
        function onCodexLoginStatusChanged() { root.syncCodexLoginState() }
        function onCodexLoginBusyChanged() { root.syncCodexLoginState() }
        function onCodexLoginErrorChanged() { root.syncCodexLoginState() }
        function onCodexDeviceCodeChanged() { root.syncCodexLoginState() }
        function onCodexAccountsChanged() { root.syncCodexLoginState() }
        function onCodexSnapshotChanged() { root.syncCodexLoginState() }
        function onCodexUsageLoadingChanged() { root.syncCodexLoginState() }
    }

    ListModel {
        id: providersModel
    }

    title: root.editorVisible
        ? (root.creatingProvider
           ? qsTr("添加供应商")
           : qsTr("编辑 %1").arg(providerEditor.candidate.providerName || qsTr("供应商")))
        : qsTr("供应商")

    titleDelegate: Component {
        RowLayout {
            width: parent ? parent.width : implicitWidth
            height: parent ? parent.height : implicitHeight
            spacing: Kirigami.Units.smallSpacing

            QQC2.ToolButton {
                objectName: "providerBackButton"
                visible: root.editorVisible
                icon.name: "go-previous"
                display: QQC2.AbstractButton.IconOnly
                Accessible.name: qsTr("返回供应商列表")
                QQC2.ToolTip.text: Accessible.name
                QQC2.ToolTip.visible: hovered
                onClicked: root.cancelEditor()
            }

            Kirigami.Separator {
                visible: root.editorVisible
                Layout.preferredHeight: Kirigami.Units.gridUnit
            }

            Kirigami.Heading {
                objectName: "providerPageTitle"
                Layout.fillWidth: true
                level: 2
                text: root.title
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

                Item { Layout.fillWidth: true }

                QQC2.Button {
                    objectName: "addProviderButton"
                    text: qsTr("添加供应商")
                    icon.name: "list-add"
                    onClicked: root.beginAdd()
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: qsTr("选择供应商以查看内置限额，或编辑自定义查询契约。")
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
                    required property int index
                    required property bool enabled
                    required property string logoSource
                    required property string logoChar

                    Layout.fillWidth: true
                    onClicked: root.beginEdit(providerDelegate.providerId)

                    contentItem: RowLayout {
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

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

                        Rectangle {
                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                            Layout.preferredHeight: Layout.preferredWidth
                            radius: width / 2
                            color: Kirigami.Theme.alternateBackgroundColor
                            Image {
                                id: logoThumb
                                anchors.fill: parent
                                anchors.margins: 1
                                source: providerDelegate.logoSource
                                fillMode: Image.PreserveAspectFit
                                visible: status === Image.Ready
                            }
                            QQC2.Label {
                                anchors.centerIn: parent
                                visible: logoThumb.status !== Image.Ready
                                text: providerDelegate.logoChar
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }

                        QQC2.Switch {
                            objectName: "providerEnabledSwitch"
                            checked: providerDelegate.enabled
                            Accessible.name: qsTr("启用 %1").arg(providerDelegate.providerName)
                            onToggled: {
                                const index = root.indexForId(providerDelegate.providerId)
                                if (index < 0) return
                                const def = JSON.parse(providersModel.get(index).definitionJson)
                                def.enabled = checked
                                providersModel.set(index, definitionRow(def))
                                Qt.callLater(root.syncWorkingValue)
                            }
                        }

                        QQC2.ToolButton {
                            icon.name: "go-up"
                            enabled: providerDelegate.index > 0
                            Accessible.name: qsTr("上移 %1").arg(providerDelegate.providerName)
                            QQC2.ToolTip.text: Accessible.name
                            QQC2.ToolTip.visible: hovered
                            onClicked: root.moveProvider(providerDelegate.providerId, -1)
                        }

                        QQC2.ToolButton {
                            icon.name: "go-down"
                            enabled: providerDelegate.index < providersModel.count - 1
                            Accessible.name: qsTr("下移 %1").arg(providerDelegate.providerName)
                            QQC2.ToolTip.text: Accessible.name
                            QQC2.ToolTip.visible: hovered
                            onClicked: root.moveProvider(providerDelegate.providerId, 1)
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
                highlighterBackend: root.usageBackend
                credentialConfigured: root.miniMaxCredentialConfigured
                credentialBusy: root.miniMaxCredentialBusy
                credentialError: root.miniMaxCredentialError
                credentialStatus: root.miniMaxCredentialStatus
                miniMaxUsageLoading: root.miniMaxUsageLoading
                miniMaxUsageStatus: root.miniMaxUsageStatus
                miniMaxUsageError: root.miniMaxUsageError
                codexLoggedIn: root.codexLoggedIn
                codexLoginBusy: root.codexLoginBusy
                codexLoginError: root.codexLoginError
                codexLoginStatus: root.codexLoginStatus
                codexDeviceCode: root.codexDeviceCode
                codexDeviceUrl: root.codexDeviceUrl
                codexAccounts: root.codexAccounts
                codexUsageLoading: root.codexUsageLoading
                codexUsageStatus: root.codexUsageStatus
                codexUsageError: root.codexUsageError
                onCandidateChanged: root.syncEditorCandidate()
                onSaveApiKeyRequested: apiKey => root.saveMiniMaxApiKey(apiKey)
                onClearApiKeyRequested: root.clearMiniMaxApiKey()
                onRefreshMiniMaxRequested: root.refreshMiniMaxUsage()
                onStartCodexLoginRequested: root.callCodexBackend("startCodexLogin")
                onCancelCodexLoginRequested: root.callCodexBackend("cancelCodexLogin")
                onOpenCodexLoginPageRequested: root.callCodexBackend("openCodexLoginPage")
                onRemoveCodexAccountRequested: profileId => root.removeCodexAccount(profileId)
                onRefreshCodexUsageRequested: root.callCodexBackend("refreshCodexUsage")
            }
        }
    }

    Kirigami.PromptDialog {
        id: deleteDialog

        objectName: "deleteProviderDialog"
        parent: root
        title: qsTr("删除供应商？")
        subtitle: qsTr("将从当前配置中移除该供应商及其限额项。")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        onAccepted: root.deleteProvider(root.pendingDeleteId)
    }
}
