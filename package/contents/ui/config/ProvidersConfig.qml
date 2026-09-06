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
    property string cfg_compactStyle: "bar"
    property string cfg_compactStyleDefault: "bar"
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
    property var backendOverride: null
    readonly property var usageBackend: backendOverride || Plasmoid
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
    property bool deepseekCredentialConfigured: false
    property bool deepseekCredentialBusy: false
    property bool deepseekCredentialError: false
    property string deepseekCredentialStatus: ""

    property bool deepseekUsageLoading: false
    property string deepseekUsageStatus: ""
    property string deepseekUsageError: ""
    property bool zhCredentialConfigured: false
    property bool zhCredentialBusy: false
    property bool zhCredentialError: false
    property string zhCredentialStatus: qsTr("尚未保存 API Key")
    property bool opencodeGoCredentialConfigured: false
    property bool opencodeGoCredentialBusy: false
    property bool opencodeGoCredentialError: false
    property string opencodeGoCredentialStatus: qsTr("尚未保存凭据")
    property bool opencodeGoUsageLoading: false
    property string opencodeGoUsageStatus: qsTr("未配置")
    property string opencodeGoUsageError: ""
    property bool agnesCredentialConfigured: false
    property bool agnesCredentialBusy: false
    property bool agnesCredentialError: false
    property string agnesCredentialStatus: qsTr("尚未保存 API Key")
    property bool agnesUsageLoading: false
    property string agnesUsageStatus: qsTr("未配置")
    property string agnesUsageError: ""
    property bool commandCodeCredentialConfigured: false
    property bool commandCodeCredentialBusy: false
    property bool commandCodeCredentialError: false
    property string commandCodeCredentialStatus: qsTr("尚未保存 Cookie")
    property bool commandCodeUsageLoading: false
    property string commandCodeUsageStatus: qsTr("未配置")
    property string commandCodeUsageError: ""
    property bool zhUsageLoading: false
    property string zhUsageStatus: qsTr("未配置")
    property string zhUsageError: ""
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
    property string cfg_sortMode: "default"
    property string cfg_sortModeDefault: "default"

    verticalScrollBarPolicy: QQC2.ScrollBar.AsNeeded

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
        const currentDefinitions = definitions()
        cfg_providers = ProviderConfig.serializeDefinitions(currentDefinitions)
        cfg_customOrder = JSON.stringify(currentDefinitions.map(function(definition) {
            return definition.id
        }))
    }

    function addProvider(candidate) {
        const result = ProviderConfig.validateProvider(candidate, definitions())
        if (!result.valid)
            return false
        providersModel.append(definitionRow(copy(candidate)))
        syncWorkingValue()
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

    function moveProvider(id, offset) {
        const index = indexForId(id)
        const destination = index + offset
        if (index < 0 || destination < 0 || destination >= providersModel.count)
            return false
        providersModel.move(index, destination, 1)
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
        if (editorVisible && !syncEditorCandidate()) {
            console.warn("AIQuotaPilot: provider save rejected:",
                         providerEditor.validation.message)
            return false
        }
        syncWorkingValue()
        const saveShared = usageBackend["saveSharedProviders"]
        if (typeof saveShared !== "function") {
            console.warn("AIQuotaPilot: shared provider backend is unavailable")
            return false
        }
        return saveShared.call(usageBackend, cfg_providers) === true
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

    function credentialBackendMethod(catalogId, action) {
        if (catalogId === "codexzh") {
            if (action === "save") return "saveCodexZhApiKey"
            if (action === "clear") return "clearCodexZhApiKey"
            if (action === "refresh") return "refreshCodexZhUsage"
        } else if (catalogId === "minimax") {
            if (action === "save") return "saveMiniMaxApiKey"
            if (action === "clear") return "clearMiniMaxApiKey"
            if (action === "refresh") return "refreshMiniMax"
        } else if (catalogId === "deepseek") {
            if (action === "save") return "saveDeepSeekApiKey"
            if (action === "clear") return "clearDeepSeekApiKey"
            if (action === "refresh") return "refreshDeepSeekUsage"
        } else if (catalogId === "opencode-go") {
            if (action === "save") return "saveOpenCodeGoCredential"
            if (action === "clear") return "clearOpenCodeGoCredential"
            if (action === "refresh") return "refreshOpenCodeGoUsage"
        } else if (catalogId === "agnes-ai") {
            if (action === "save") return "saveAgnesApiKey"
            if (action === "clear") return "clearAgnesApiKey"
            if (action === "refresh") return "refreshAgnesUsage"
        } else if (catalogId === "command-code") {
            if (action === "save") return "saveCommandCodeCookie"
            if (action === "clear") return "clearCommandCodeCookie"
            if (action === "refresh") return "refreshCommandCodeUsage"
        }
        return ""
    }


    function callCredentialBackend(catalogId, action, apiKey) {
        const operation = usageBackend[credentialBackendMethod(catalogId, action)]
        if (typeof operation !== "function") {
            if (catalogId === "codexzh") {
                zhCredentialError = true
                zhCredentialStatus = qsTr("CodexZH 凭据后端未加载，请重启 Plasma 后重试")
            } else if (catalogId === "deepseek") {
                deepseekCredentialError = true
                deepseekCredentialStatus = qsTr("DeepSeek 凭据后端未加载，请重启 Plasma 后重试")
            } else if (catalogId === "agnes-ai") {
                agnesCredentialError = true
                agnesCredentialStatus = qsTr("Agnes AI 凭据后端未加载，请重启 Plasma 后重试")
            } else if (catalogId === "command-code") {
                commandCodeCredentialError = true
                commandCodeCredentialStatus = qsTr("Command Code 凭据后端未加载，请重启 Plasma 后重试")
            } else {
                miniMaxCredentialError = true
                miniMaxCredentialStatus = qsTr("MiniMax 凭据后端未加载，请重启 Plasma 后重试")
            }
            return false
        }
        if (typeof apiKey === "string")
            operation.call(usageBackend, apiKey)
        else
            operation.call(usageBackend)
        return true
    }

    function syncCodexZhState() {
        const prefix = "codexzh"
        zhCredentialConfigured = usageBackend[prefix + "CredentialConfigured"] === true
        zhCredentialBusy = usageBackend[prefix + "CredentialBusy"] === true
        zhCredentialError = usageBackend[prefix + "CredentialError"] === true
        const status = usageBackend[prefix + "CredentialStatus"]
        const snapshot = usageBackend[prefix + "Snapshot"]
        zhCredentialStatus = typeof status === "string" && status.length > 0
            ? status : qsTr("尚未保存 API Key")
        zhUsageLoading = usageBackend[prefix + "Loading"] === true
        zhUsageStatus = snapshot && typeof snapshot.statusLabel === "string"
            ? snapshot.statusLabel : qsTr("未配置")
        zhUsageError = snapshot && typeof snapshot.errorText === "string"
            ? snapshot.errorText : ""
    }

    function connectCodexZhSignals() {
        const prefix = "codexzh"
        const suffixes = [
            "CredentialConfiguredChanged", "CredentialStatusChanged",
            "CredentialBusyChanged", "CredentialErrorChanged",
            "SnapshotChanged", "LoadingChanged"
        ]
        for (let i = 0; i < suffixes.length; ++i) {
            const signal = usageBackend[prefix + suffixes[i]]
            if (signal && typeof signal.connect === "function")
                signal.connect(root.syncCodexZhState)
        }
    }

    function syncDeepSeekState() {
        const prefix = "deepseek"
        deepseekCredentialConfigured = usageBackend[prefix + "CredentialConfigured"] === true
        deepseekCredentialBusy = usageBackend[prefix + "CredentialBusy"] === true
        deepseekCredentialError = usageBackend[prefix + "CredentialError"] === true
        const status = usageBackend[prefix + "CredentialStatus"]
        const snapshot = usageBackend[prefix + "Snapshot"]
        deepseekCredentialStatus = typeof status === "string" && status.length > 0
            ? status : qsTr("尚未保存 API Key")
        deepseekUsageLoading = usageBackend[prefix + "Loading"] === true
        deepseekUsageStatus = snapshot && typeof snapshot.statusLabel === "string"
            ? snapshot.statusLabel : qsTr("未配置")
        deepseekUsageError = snapshot && typeof snapshot.errorText === "string"
            ? snapshot.errorText : ""
    }

    function connectDeepSeekSignals() {
        const prefix = "deepseek"
        const suffixes = [
            "CredentialConfiguredChanged", "CredentialStatusChanged",
            "CredentialBusyChanged", "CredentialErrorChanged",
            "SnapshotChanged", "LoadingChanged"
        ]
        for (let i = 0; i < suffixes.length; ++i) {
            const signal = usageBackend[prefix + suffixes[i]]
            if (signal && typeof signal.connect === "function")
                signal.connect(root.syncDeepSeekState)
        }
    }

    function syncOpenCodeGoState() {
        const prefix = "opencodeGo"
        opencodeGoCredentialConfigured = usageBackend[prefix + "CredentialConfigured"] === true
        opencodeGoCredentialBusy = usageBackend[prefix + "CredentialBusy"] === true
        opencodeGoCredentialError = usageBackend[prefix + "CredentialError"] === true
        const status = usageBackend[prefix + "CredentialStatus"]
        const snapshot = usageBackend[prefix + "Snapshot"]
        opencodeGoCredentialStatus = typeof status === "string" && status.length > 0
            ? status : qsTr("尚未保存凭据")
        opencodeGoUsageLoading = usageBackend[prefix + "Loading"] === true
        opencodeGoUsageStatus = snapshot && typeof snapshot.statusLabel === "string"
            ? snapshot.statusLabel : qsTr("未配置")
        opencodeGoUsageError = snapshot && typeof snapshot.errorText === "string"
            ? snapshot.errorText : ""
    }

    function connectOpenCodeGoSignals() {
        const prefix = "opencodeGo"
        const suffixes = [
            "CredentialConfiguredChanged", "CredentialStatusChanged",
            "CredentialBusyChanged", "CredentialErrorChanged",
            "SnapshotChanged", "LoadingChanged"
        ]
        for (let i = 0; i < suffixes.length; ++i) {
            const signal = usageBackend[prefix + suffixes[i]]
            if (signal && typeof signal.connect === "function")
                signal.connect(root.syncOpenCodeGoState)
        }
    }

    // Agnes AI：单值 API Key，与 MiniMax 相同的状态轮询模式（前缀 agnes）。
    function syncAgnesState() {
        const prefix = "agnes"
        agnesCredentialConfigured = usageBackend[prefix + "CredentialConfigured"] === true
        agnesCredentialBusy = usageBackend[prefix + "CredentialBusy"] === true
        agnesCredentialError = usageBackend[prefix + "CredentialError"] === true
        const status = usageBackend[prefix + "CredentialStatus"]
        const snapshot = usageBackend[prefix + "Snapshot"]
        agnesCredentialStatus = typeof status === "string" && status.length > 0
            ? status : qsTr("尚未保存 API Key")
        agnesUsageLoading = usageBackend[prefix + "Loading"] === true
        agnesUsageStatus = snapshot && typeof snapshot.statusLabel === "string"
            ? snapshot.statusLabel : qsTr("未配置")
        agnesUsageError = snapshot && typeof snapshot.errorText === "string"
            ? snapshot.errorText : ""
    }

    function connectAgnesSignals() {
        const prefix = "agnes"
        const suffixes = [
            "CredentialConfiguredChanged", "CredentialStatusChanged",
            "CredentialBusyChanged", "CredentialErrorChanged",
            "SnapshotChanged", "LoadingChanged"
        ]
        for (let i = 0; i < suffixes.length; ++i) {
            const signal = usageBackend[prefix + suffixes[i]]
            if (signal && typeof signal.connect === "function")
                signal.connect(root.syncAgnesState)
        }
    }

    // Command Code：单值 Cookie，状态轮询模式与 AGNES 相同（前缀 commandCode）。
    function syncCommandCodeState() {
        const prefix = "commandCode"
        commandCodeCredentialConfigured = usageBackend[prefix + "CredentialConfigured"] === true
        commandCodeCredentialBusy = usageBackend[prefix + "CredentialBusy"] === true
        commandCodeCredentialError = usageBackend[prefix + "CredentialError"] === true
        const status = usageBackend[prefix + "CredentialStatus"]
        const snapshot = usageBackend[prefix + "Snapshot"]
        commandCodeCredentialStatus = typeof status === "string" && status.length > 0
            ? status : qsTr("尚未保存 Cookie")
        commandCodeUsageLoading = usageBackend[prefix + "Loading"] === true
        commandCodeUsageStatus = snapshot && typeof snapshot.statusLabel === "string"
            ? snapshot.statusLabel : qsTr("未配置")
        commandCodeUsageError = snapshot && typeof snapshot.errorText === "string"
            ? snapshot.errorText : ""
    }

    function connectCommandCodeSignals() {
        const prefix = "commandCode"
        const suffixes = [
            "CredentialConfiguredChanged", "CredentialStatusChanged",
            "CredentialBusyChanged", "CredentialErrorChanged",
            "SnapshotChanged", "LoadingChanged"
        ]
        for (let i = 0; i < suffixes.length; ++i) {
            const signal = usageBackend[prefix + suffixes[i]]
            if (signal && typeof signal.connect === "function")
                signal.connect(root.syncCommandCodeState)
        }
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
        const accountList = []
        if (accounts && typeof accounts.length === "number") {
            for (let index = 0; index < accounts.length; ++index)
                accountList.push(accounts[index])
        }
        codexAccounts = accountList
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
        const sharedProviders = usageBackend["sharedProviders"]
        if (typeof sharedProviders === "string" && sharedProviders.length > 0)
            cfg_providers = sharedProviders
        const parsed = ProviderConfig.parseWorkingDefinitions(cfg_providers)
        for (let i = 0; i < parsed.length; ++i)
            providersModel.append(definitionRow(parsed[i]))
        syncCredentialState()
        syncCodexZhState()
        connectCodexZhSignals()
        syncDeepSeekState()
        connectDeepSeekSignals()
        syncOpenCodeGoState()
        connectOpenCodeGoSignals()
        syncAgnesState()
        connectAgnesSignals()
        syncCommandCodeState()
        connectCommandCodeSignals()
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
        function onDeepseekCredentialConfiguredChanged() { root.syncDeepSeekState() }
        function onDeepseekCredentialStatusChanged() { root.syncDeepSeekState() }
        function onDeepseekCredentialBusyChanged() { root.syncDeepSeekState() }
        function onDeepseekCredentialErrorChanged() { root.syncDeepSeekState() }
        function onDeepseekSnapshotChanged() { root.syncDeepSeekState() }
        function onDeepseekLoadingChanged() { root.syncDeepSeekState() }
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

    Item {
        objectName: "providerStackLayout"
        width: parent.width
        implicitHeight: root.editorVisible ? editorPage.implicitHeight : providerListPage.implicitHeight

        StackLayout {
            anchors.fill: parent
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
                            // 直接读 ListModel role：delegate 不能声明同名 required property，
                            // 会遮蔽 QQuickItem::enabled（qmllint property-override）
                            checked: {
                                const providerIndex = root.indexForId(providerDelegate.providerId)
                                return providerIndex >= 0
                                    && providersModel.get(providerIndex).enabled
                            }
                            Accessible.name: qsTr("启用 %1").arg(providerDelegate.providerName)
                            onToggled: {
                                const index = root.indexForId(providerDelegate.providerId)
                                if (index < 0) return
                                const def = JSON.parse(providersModel.get(index).definitionJson)
                                def.enabled = checked
                                providersModel.set(index, root.definitionRow(def))
                                root.syncWorkingValue()
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
                credentialConfigured: providerEditor.isCommandCode
                    ? root.commandCodeCredentialConfigured
                    : (providerEditor.isAgnes
                       ? root.agnesCredentialConfigured
                       : (providerEditor.isOpenCodeGo
                          ? root.opencodeGoCredentialConfigured
                          : (providerEditor.isCodexZh
                             ? root.zhCredentialConfigured
                             : (providerEditor.isDeepSeek
                                ? root.deepseekCredentialConfigured
                                : root.miniMaxCredentialConfigured))))
                credentialBusy: providerEditor.isCommandCode
                    ? root.commandCodeCredentialBusy
                    : (providerEditor.isAgnes
                       ? root.agnesCredentialBusy
                       : (providerEditor.isOpenCodeGo
                          ? root.opencodeGoCredentialBusy
                          : (providerEditor.isCodexZh
                             ? root.zhCredentialBusy
                             : (providerEditor.isDeepSeek
                                ? root.deepseekCredentialBusy
                                : root.miniMaxCredentialBusy))))
                credentialError: providerEditor.isCommandCode
                    ? root.commandCodeCredentialError
                    : (providerEditor.isAgnes
                       ? root.agnesCredentialError
                       : (providerEditor.isOpenCodeGo
                          ? root.opencodeGoCredentialError
                          : (providerEditor.isCodexZh
                             ? root.zhCredentialError
                             : (providerEditor.isDeepSeek
                                ? root.deepseekCredentialError
                                : root.miniMaxCredentialError))))
                credentialStatus: providerEditor.isCommandCode
                    ? root.commandCodeCredentialStatus
                    : (providerEditor.isAgnes
                       ? root.agnesCredentialStatus
                       : (providerEditor.isOpenCodeGo
                          ? root.opencodeGoCredentialStatus
                          : (providerEditor.isCodexZh
                             ? root.zhCredentialStatus
                             : (providerEditor.isDeepSeek
                                ? root.deepseekCredentialStatus
                                : root.miniMaxCredentialStatus))))
                miniMaxUsageLoading: root.miniMaxUsageLoading
                miniMaxUsageStatus: root.miniMaxUsageStatus
                miniMaxUsageError: root.miniMaxUsageError
                codexzhUsageLoading: root.zhUsageLoading
                codexzhUsageStatus: root.zhUsageStatus
                codexzhUsageError: root.zhUsageError
                deepseekUsageLoading: root.deepseekUsageLoading
                deepseekUsageStatus: root.deepseekUsageStatus
                deepseekUsageError: root.deepseekUsageError
                opencodeGoUsageLoading: root.opencodeGoUsageLoading
                opencodeGoUsageStatus: root.opencodeGoUsageStatus
                opencodeGoUsageError: root.opencodeGoUsageError
                agnesUsageLoading: root.agnesUsageLoading
                agnesUsageStatus: root.agnesUsageStatus
                agnesUsageError: root.agnesUsageError
                commandCodeUsageLoading: root.commandCodeUsageLoading
                commandCodeUsageStatus: root.commandCodeUsageStatus
                commandCodeUsageError: root.commandCodeUsageError
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
                onSaveApiKeyRequested: apiKey => root.callCredentialBackend(
                    providerEditor.candidate.catalogId, "save", apiKey)
                onClearApiKeyRequested: root.callCredentialBackend(
                    providerEditor.candidate.catalogId, "clear")
                onRefreshMiniMaxRequested: root.callCredentialBackend("minimax", "refresh")
                onRefreshCodexZhRequested: root.callCredentialBackend("codexzh", "refresh")
                onRefreshDeepSeekRequested: root.callCredentialBackend("deepseek", "refresh")
                onSaveOpenCodeGoCredentialRequested: (workspaceId, cookie) => {
                    const operation = root.usageBackend["saveOpenCodeGoCredential"]
                    if (typeof operation === "function")
                        operation.call(root.usageBackend, workspaceId, cookie)
                }
                onClearOpenCodeGoCredentialRequested: {
                    const operation = root.usageBackend["clearOpenCodeGoCredential"]
                    if (typeof operation === "function")
                        operation.call(root.usageBackend)
                }
                onRefreshOpenCodeGoRequested: root.callCredentialBackend("opencode-go", "refresh")
                onSaveCommandCodeCredentialRequested: cookie => root.callCredentialBackend(
                    "command-code", "save", cookie)
                onClearCommandCodeCredentialRequested: root.callCredentialBackend(
                    "command-code", "clear")
                onRefreshCommandCodeRequested: root.callCredentialBackend("command-code", "refresh")
                onRefreshAgnesRequested: root.callCredentialBackend("agnes-ai", "refresh")
                onStartCodexLoginRequested: root.callCodexBackend("startCodexLogin")
                onCancelCodexLoginRequested: root.callCodexBackend("cancelCodexLogin")
                onOpenCodexLoginPageRequested: root.callCodexBackend("openCodexLoginPage")
                onRemoveCodexAccountRequested: profileId => root.removeCodexAccount(profileId)
                onRefreshCodexUsageRequested: root.callCodexBackend("refreshCodexUsage")
            }
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
