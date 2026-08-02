// SPDX-License-Identifier: GPL-2.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "../../js/providerCatalog.js" as ProviderCatalog
import "../../js/providerConfig.js" as ProviderConfig
import "../../js/providerRegistry.js" as ProviderRegistry
import "../../js/scriptTools.js" as ScriptTools

Item {
    id: root

    objectName: "providerEditor"
    implicitWidth: form.implicitWidth
    implicitHeight: form.implicitHeight

    property var candidate: ({})
    property var quotaModel: []
    property var siblings: []
    property var highlighterBackend: null
    property var customDraft: ({})
    property bool editingExisting: false
    property bool credentialConfigured: false
    property bool credentialBusy: false
    property bool credentialError: false
    property string credentialStatus: qsTr("尚未保存 API Key")
    property bool miniMaxUsageLoading: false
    property string miniMaxUsageStatus: qsTr("未配置")
    property string miniMaxUsageError: ""
    property bool codexzhUsageLoading: false
    property string codexzhUsageStatus: qsTr("未配置")
    property string codexzhUsageError: ""
    property bool deepseekUsageLoading: false
    property string deepseekUsageStatus: qsTr("未配置")
    property string deepseekUsageError: ""
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
    property string scriptTestMessage: ""
    property bool scriptTestError: false
    readonly property bool isCustom: (candidate.catalogId || "custom") === "custom"
    readonly property bool isMiniMax: (candidate.catalogId || "") === "minimax"
    readonly property bool isCodex: (candidate.catalogId || "") === "codex"
    readonly property bool isCodexZh: (candidate.catalogId || "") === "codexzh"
    readonly property bool isDeepSeek: (candidate.catalogId || "") === "deepseek"
    readonly property var validation: ProviderConfig.validateProvider(candidate, siblings)
    readonly property var providerOptions: ProviderCatalog.providerOptions()
    readonly property real fieldWidth: Kirigami.Units.gridUnit * 20
    readonly property real formWidth: Kirigami.Units.gridUnit * 30
    readonly property real minimumScriptEditorHeight: Kirigami.Units.gridUnit * 8
    property real scriptEditorHeight: Kirigami.Units.gridUnit * 15

    signal saveApiKeyRequested(string apiKey)
    signal clearApiKeyRequested()
    signal refreshMiniMaxRequested()
    signal refreshCodexZhRequested()
    signal refreshDeepSeekRequested()
    signal startCodexLoginRequested()
    signal cancelCodexLoginRequested()
    signal openCodexLoginPageRequested()
    signal removeCodexAccountRequested(string profileId)
    signal refreshCodexUsageRequested()

    function copy(value) {
        return JSON.parse(JSON.stringify(value))
    }

    function blankCustomDefinition() {
        return {
            catalogId: "custom",
            id: "provider-custom",
            providerName: "",
            website: "",
            vendor: "自定义",
            sourceLabel: "自定义",
            trustMode: "strict",
            template: ProviderCatalog.DEFAULT_TEMPLATE,
            script: ScriptTools.DEFAULT_SCRIPT,
            logoPath: "",
            plans: [{
                id: "quota-1",
                planName: "",
                unit: "",
                sourceType: "http-js",
                usedVariable: "${used}",
                limitVariable: "${limit}"
            }]
        }
    }

    function setCandidate(value, existing) {
        candidate = copy(value)
        quotaModel = candidate.plans || []
        editingExisting = existing === true
        customDraft = isCustom ? copy(candidate) : blankCustomDefinition()
        scriptTestMessage = ""
        apiKeyField.clear()
        Qt.callLater(attachHighlighter)
    }

    function currentCandidate() {
        return copy(candidate)
    }

    function rememberCustom() {
        if (isCustom)
            customDraft = copy(candidate)
    }

    function updateField(name, value) {
        const plans = candidate.plans
        const next = copy(candidate)
        next[name] = value
        next.plans = plans
        candidate = next
        rememberCustom()
        scriptTestMessage = ""
    }

    function updatePlan(index, name, value) {
        candidate.plans[index][name] = value
        candidateChanged()
        rememberCustom()
        scriptTestMessage = ""
    }

    function selectCatalog(catalogId) {
        if (catalogId === (candidate.catalogId || "custom"))
            return
        if (isCustom)
            customDraft = copy(candidate)
        candidate = catalogId === "custom"
            ? copy(customDraft)
            : ProviderCatalog.definitionFor(catalogId)
        quotaModel = candidate.plans || []
        scriptTestMessage = ""
        Qt.callLater(attachHighlighter)
    }

    function addPlan() {
        const next = copy(candidate)
        let number = next.plans.length + 1
        let id = "quota-" + number
        while (next.plans.some(function(plan) { return plan.id === id })) {
            ++number
            id = "quota-" + number
        }
        next.plans.push({
            id: id,
            planName: "",
            unit: "",
            sourceType: "http-js",
            usedVariable: "${used" + number + "}",
            limitVariable: "${limit" + number + "}"
        })
        candidate = next
        quotaModel = candidate.plans
        rememberCustom()
    }

    function removePlan(index) {
        const next = copy(candidate)
        next.plans.splice(index, 1)
        candidate = next
        quotaModel = candidate.plans
        rememberCustom()
    }

    function attachHighlighter() {
        const attach = highlighterBackend
            ? highlighterBackend["attachJavaScriptHighlighter"] : null
        if (typeof attach === "function") {
            attach.call(highlighterBackend, scriptArea.textDocument,
                        Kirigami.Theme.linkColor,
                        Kirigami.Theme.positiveTextColor,
                        Kirigami.Theme.disabledTextColor,
                        Kirigami.Theme.neutralTextColor)
        }
    }

    function formatScript() {
        updateField("script", ScriptTools.formatJavaScript(candidate.script || ""))
    }

    function setScriptEditorHeight(height) {
        const numericHeight = Number(height)
        scriptEditorHeight = Math.max(minimumScriptEditorHeight,
                                      Number.isFinite(numericHeight)
                                      ? numericHeight : minimumScriptEditorHeight)
    }

    function testScriptContract() {
        const result = ScriptTools.validateContract(candidate.script, candidate.plans)
        scriptTestError = !result.valid
        scriptTestMessage = result.valid
            ? qsTr("契约验证通过；识别变量：%1。保存或应用设置后，刷新将执行真实查询。")
                .arg(result.variables.join("、"))
            : result.message
    }

    component FieldLabel: QQC2.Label {
        Layout.preferredWidth: Kirigami.Units.gridUnit * 9
        Layout.alignment: Qt.AlignRight | Qt.AlignVCenter
        horizontalAlignment: Text.AlignRight
    }

    component SectionHeading: RowLayout {
        property alias text: heading.text

        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Heading {
            id: heading

            level: 3
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }
    }

    ColumnLayout {
        id: form

        objectName: "providerForm"
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Kirigami.Units.largeSpacing

        SectionHeading {
            text: qsTr("基本信息")
        }

        // 64x64 居中 Logo 头像
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.formWidth
            Layout.maximumWidth: root.formWidth

            Item { Layout.fillWidth: true }

            Rectangle {
                id: logoAvatar

                Layout.preferredWidth: Kirigami.Units.iconSizes.medium * 2
                Layout.preferredHeight: Layout.preferredWidth
                radius: width / 2
                color: Kirigami.Theme.alternateBackgroundColor

                Image {
                    id: logoImage

                    anchors.fill: parent
                    anchors.margins: 2
                    source: {
                        const catalogId = root.candidate.catalogId || ""
                        if (root.candidate.logoPath && root.candidate.logoPath.length > 0)
                            return root.candidate.logoPath
                        if (catalogId.length > 0 && !root.isCustom) {
                            const svg = ProviderRegistry.logoSvgFor(catalogId)
                            if (svg && svg.length > 0)
                                return "data:image/svg+xml;utf8," + svg
                        }
                        return ""
                    }
                    fillMode: Image.PreserveAspectFit
                    visible: status === Image.Ready
                    asynchronous: true
                }

                QQC2.Label {
                    anchors.centerIn: parent
                    visible: logoImage.status !== Image.Ready
                    text: (root.candidate.providerName || "").trim().charAt(0).toUpperCase()
                    color: Kirigami.Theme.disabledTextColor
                    font.pixelSize: Kirigami.Theme.defaultFont.pixelSize * 2
                }

                MouseArea {
                    id: logoClickArea

                    anchors.fill: parent
                    cursorShape: root.isCustom ? Qt.PointingHandCursor : Qt.ArrowCursor
                    enabled: root.isCustom
                    hoverEnabled: root.isCustom
                    onClicked: logoFileDialog.open()

                    QQC2.ToolTip {
                        visible: parent.containsMouse && root.isCustom
                        text: qsTr("点击选择供应商 Logo")
                    }
                }
            }

            FileDialog {
                id: logoFileDialog

                title: qsTr("选择供应商 Logo")
                nameFilters: [qsTr("图片文件 (*.png *.jpg *.jpeg *.svg *.bmp *.gif)")]
                onAccepted: {
                    if (selectedFile)
                        root.updateField("logoPath", selectedFile)
                }
            }

            Item { Layout.fillWidth: true }
        }

        GridLayout {
            id: basicForm

            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.formWidth
            Layout.maximumWidth: root.formWidth
            columns: 2
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.smallSpacing

            FieldLabel { text: qsTr("厂商选择：") }

            QQC2.ComboBox {
                objectName: "providerCatalogField"
                Layout.preferredWidth: root.fieldWidth
                Layout.maximumWidth: root.fieldWidth
                model: root.providerOptions
                textRole: "text"
                valueRole: "value"
                currentIndex: Math.max(0, root.providerOptions.findIndex(function(option) {
                    return option.value === (root.candidate.catalogId || "custom")
                }))
                onActivated: root.selectCatalog(currentValue)
            }

            FieldLabel { text: qsTr("供应商标识：") }

            QQC2.TextField {
                objectName: "providerIdField"
                Layout.preferredWidth: root.fieldWidth
                Layout.maximumWidth: root.fieldWidth
                text: root.candidate.id || ""
                readOnly: !root.isCustom
                onTextEdited: root.updateField("id", text)
            }

            FieldLabel { text: qsTr("供应商名称：") }

            QQC2.TextField {
                objectName: "providerNameField"
                Layout.preferredWidth: root.fieldWidth
                Layout.maximumWidth: root.fieldWidth
                text: root.candidate.providerName || ""
                readOnly: !root.isCustom
                onTextEdited: root.updateField("providerName", text)
            }

            FieldLabel { text: qsTr("官网链接：") }

            QQC2.TextField {
                objectName: "providerWebsiteField"
                Layout.preferredWidth: root.fieldWidth
                Layout.maximumWidth: root.fieldWidth
                text: root.candidate.website || ""
                readOnly: !root.isCustom
                placeholderText: "https://example.com/"
                inputMethodHints: Qt.ImhUrlCharactersOnly
                onTextEdited: root.updateField("website", text)
            }

            FieldLabel { text: qsTr("套餐/订阅价格：") }

            QQC2.TextField {
                objectName: "priceField"
                Layout.preferredWidth: root.fieldWidth
                Layout.maximumWidth: root.fieldWidth
                placeholderText: qsTr("可选，单位 ¥，如 30 或 19.9")
                text: (typeof root.candidate.price === "number" && root.candidate.price > 0)
                    ? String(root.candidate.price) : ""
                validator: DoubleValidator {
                    bottom: 0
                    decimals: 2
                }
                onTextChanged: {
                    const parsed = text.trim() === "" ? 0 : Number(text)
                    root.updateField("price", isFinite(parsed) ? parsed : 0)
                }
            }

        }

        Kirigami.InlineMessage {
            visible: !root.isCustom && !root.isMiniMax && !root.isCodex && !root.isCodexZh && !root.isDeepSeek
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.formWidth
            Layout.maximumWidth: root.formWidth
            text: qsTr("该厂商的套餐结构已内置；当前版本尚未接入其凭据查询，运行时会明确显示“暂无用量”。")
            type: Kirigami.MessageType.Information
        }

        SectionHeading {
            visible: root.isCodex
            text: qsTr("Codex 登录")
        }

        ColumnLayout {
            objectName: "codexLoginSection"
            visible: root.isCodex
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.formWidth
            Layout.maximumWidth: root.formWidth
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                visible: root.codexAccounts.length > 0
                text: qsTr("已登录账号")
            }

            Repeater {
                model: root.codexAccounts

                delegate: QQC2.Frame {
                    id: accountFrame

                    required property var modelData

                    objectName: "codexAccountRow"
                    Layout.fillWidth: true

                    contentItem: RowLayout {
                        Kirigami.Icon {
                            source: "user-identity"
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: width
                        }

                        QQC2.Label {
                            Layout.fillWidth: true
                            text: accountFrame.modelData.login || qsTr("ChatGPT 账号")
                            elide: Text.ElideMiddle
                        }

                        QQC2.Label {
                            visible: accountFrame.modelData.isDefault === true
                            text: qsTr("默认")
                            color: Kirigami.Theme.disabledTextColor
                        }

                        QQC2.ToolButton {
                            objectName: "removeCodexAccountButton"
                            icon.name: "edit-delete"
                            enabled: !root.codexLoginBusy
                            Accessible.name: qsTr("移除账号 %1").arg(
                                                 accountFrame.modelData.login || "")
                            QQC2.ToolTip.text: Accessible.name
                            QQC2.ToolTip.visible: hovered
                            onClicked: root.removeCodexAccountRequested(
                                           accountFrame.modelData.profileId)
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                QQC2.Button {
                    objectName: "startCodexLoginButton"
                    Layout.fillWidth: true
                    text: root.codexAccounts.length > 0
                        ? qsTr("添加其他账号") : qsTr("登录 Codex")
                    icon.name: "list-add"
                    enabled: !root.codexLoginBusy
                    onClicked: root.startCodexLoginRequested()
                }

                QQC2.Button {
                    visible: root.codexLoggedIn
                    text: root.codexUsageLoading ? qsTr("正在刷新…") : qsTr("刷新额度")
                    icon.name: "view-refresh"
                    enabled: !root.codexUsageLoading && !root.codexLoginBusy
                    onClicked: root.refreshCodexUsageRequested()
                }
            }

            RowLayout {
                visible: root.codexLoginBusy
                Layout.fillWidth: true

                QQC2.Button {
                    objectName: "openCodexLoginPageButton"
                    text: qsTr("打开浏览器")
                    icon.name: "internet-web-browser"
                    enabled: true
                    onClicked: root.openCodexLoginPageRequested()
                }

                QQC2.Button {
                    text: qsTr("取消")
                    icon.name: "dialog-cancel"
                    onClicked: root.cancelCodexLoginRequested()
                }

                Item { Layout.fillWidth: true }

                QQC2.BusyIndicator {
                    visible: root.codexLoginBusy
                    running: visible
                    implicitWidth: Kirigami.Units.iconSizes.small
                    implicitHeight: width
                }
            }

            QQC2.TextField {
                objectName: "codexDeviceCodeField"
                visible: root.codexDeviceCode.length > 0
                Layout.fillWidth: true
                text: root.codexDeviceCode
                readOnly: true
                horizontalAlignment: Text.AlignHCenter
                font.bold: true
                font.family: "monospace"
                Accessible.name: qsTr("Codex 设备验证码")
            }

            QQC2.Label {
                visible: root.codexDeviceCode.length > 0
                Layout.fillWidth: true
                text: root.codexDeviceUrl
                color: Kirigami.Theme.linkColor
                elide: Text.ElideRight
            }

            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: true
                text: root.codexLoginStatus
                type: root.codexLoginError
                    ? Kirigami.MessageType.Error
                    : (root.codexLoggedIn
                       ? Kirigami.MessageType.Positive
                       : Kirigami.MessageType.Information)
            }


            Kirigami.InlineMessage {
                Layout.fillWidth: true
                visible: root.codexLoggedIn || root.codexUsageError.length > 0
                text: root.codexUsageError.length > 0
                    ? qsTr("额度查询失败：%1").arg(root.codexUsageError)
                    : qsTr("额度查询：%1").arg(root.codexUsageStatus)
                type: root.codexUsageError.length > 0
                    ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive
            }
        }

        SectionHeading {
            visible: root.isDeepSeek
            text: qsTr("充值设置（可选）")
        }

        GridLayout {
            visible: root.isDeepSeek
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.formWidth
            Layout.maximumWidth: root.formWidth
            columns: 2
            columnSpacing: Kirigami.Units.largeSpacing

            FieldLabel { text: qsTr("充值金额：") }

            QQC2.TextField {
                objectName: "topUpAmountField"
                Layout.preferredWidth: root.fieldWidth
                Layout.maximumWidth: root.fieldWidth
                placeholderText: qsTr("可选，最近一次充值金额，单位 ¥")
                text: (typeof root.candidate.topUpAmount === "number" && root.candidate.topUpAmount > 0)
                    ? String(root.candidate.topUpAmount) : ""
                validator: DoubleValidator {
                    bottom: 0
                    decimals: 2
                }
                onTextChanged: {
                    const parsed = text.trim() === "" ? 0 : Number(text)
                    root.updateField("topUpAmount", isFinite(parsed) ? parsed : 0)
                }
            }

            FieldLabel { text: qsTr("充值时间：") }

            QQC2.TextField {
                objectName: "topUpDateField"
                Layout.preferredWidth: root.fieldWidth
                Layout.maximumWidth: root.fieldWidth
                placeholderText: qsTr("YYYY-MM-DD")
                text: (typeof root.candidate.topUpDate === "string") ? root.candidate.topUpDate : ""
                validator: RegularExpressionValidator {
                    regularExpression: /^\d{4}-\d{2}-\d{2}$/
                }
                onTextChanged: root.updateField("topUpDate", text.trim())
                // 即时反馈：失焦时非空但格式非法 → 显示提示（保存时 validateProvider 兜底）
                onEditingFinished: {
                    if (text.trim().length > 0 && !/^\d{4}-\d{2}-\d{2}$/.test(text.trim())) {
                        topUpDateHint.visible = true
                        topUpDateHint.text = qsTr("格式应为 YYYY-MM-DD，如 2026-08-01")
                    } else {
                        topUpDateHint.visible = false
                    }
                }
            }
        }

        Kirigami.InlineMessage {
            id: topUpDateHint

            objectName: "topUpDateHint"
            visible: false
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.formWidth
            Layout.maximumWidth: root.formWidth
            text: qsTr("格式应为 YYYY-MM-DD，如 2026-08-01")
            type: Kirigami.MessageType.Warning
        }

        SectionHeading {
            visible: root.isMiniMax || root.isCodexZh || root.isDeepSeek
            text: qsTr("%1 API 凭据").arg(root.isCodexZh ? "CodexZH" : (root.isDeepSeek ? "DeepSeek" : "MiniMax"))
        }

        GridLayout {
            visible: root.isMiniMax || root.isCodexZh || root.isDeepSeek
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.formWidth
            Layout.maximumWidth: root.formWidth
            columns: 2
            columnSpacing: Kirigami.Units.largeSpacing

            FieldLabel { text: qsTr("API Key：") }

            QQC2.TextField {
                id: apiKeyField

                objectName: root.isCodexZh ? "codexzhApiKeyField"
                    : (root.isDeepSeek ? "deepseekApiKeyField" : "miniMaxApiKeyField")
                Layout.preferredWidth: root.fieldWidth
                Layout.maximumWidth: root.fieldWidth
                placeholderText: root.credentialConfigured
                    ? qsTr("输入新 Key 以替换已保存凭据")
                    : (root.isCodexZh
                       ? qsTr("请输入 CodexZH API Key")
                       : (root.isDeepSeek
                          ? qsTr("请输入 DeepSeek API Key")
                          : qsTr("请输入 MiniMax API Key")))
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
        }

        RowLayout {
            visible: root.isMiniMax || root.isCodexZh || root.isDeepSeek
            Layout.fillWidth: true

            Item { Layout.fillWidth: true }

            QQC2.Button {
                objectName: root.isCodexZh ? "saveCodexZhApiKeyButton"
                    : (root.isDeepSeek ? "saveDeepSeekApiKeyButton" : "saveMiniMaxApiKeyButton")
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


            QQC2.Button {
                text: (root.isCodexZh && root.codexzhUsageLoading)
                    || (root.isMiniMax && root.miniMaxUsageLoading)
                    || (root.isDeepSeek && root.deepseekUsageLoading)
                    ? qsTr("正在刷新…")
                    : qsTr("刷新额度")
                icon.name: "view-refresh"
                enabled: root.credentialConfigured
                       && !root.miniMaxUsageLoading
                       && !root.codexzhUsageLoading
                       && !root.deepseekUsageLoading
                onClicked: root.isCodexZh
                           ? root.refreshCodexZhRequested()
                           : (root.isDeepSeek
                              ? root.refreshDeepSeekRequested()
                              : root.refreshMiniMaxRequested())
            }

            QQC2.BusyIndicator {
                visible: root.credentialBusy
                running: visible
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: width
            }
        }

        Kirigami.InlineMessage {
            objectName: root.isCodexZh ? "codexzhCredentialMessage"
                : (root.isDeepSeek ? "deepseekCredentialMessage" : "miniMaxCredentialMessage")
            visible: root.isMiniMax || root.isCodexZh || root.isDeepSeek
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.formWidth
            Layout.maximumWidth: root.formWidth
            text: root.credentialStatus + "\n" + qsTr("凭据由 KDE 钱包安全保存，不会写入小组件配置。")
            type: root.credentialError
                ? Kirigami.MessageType.Error
                : (root.credentialConfigured
                   ? Kirigami.MessageType.Positive
                   : Kirigami.MessageType.Information)
        }


        Kirigami.InlineMessage {
            visible: (root.isMiniMax || root.isCodexZh || root.isDeepSeek) && root.credentialConfigured
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: root.formWidth
            Layout.maximumWidth: root.formWidth
            text: root.isCodexZh
                ? (root.codexzhUsageError.length > 0
                   ? qsTr("额度查询失败：%1").arg(root.codexzhUsageError)
                   : qsTr("额度查询：%1").arg(root.codexzhUsageStatus))
                : (root.isDeepSeek
                   ? (root.deepseekUsageError.length > 0
                      ? qsTr("额度查询失败：%1").arg(root.deepseekUsageError)
                      : qsTr("额度查询：%1").arg(root.deepseekUsageStatus))
                   : (root.miniMaxUsageError.length > 0
                      ? qsTr("额度查询失败：%1").arg(root.miniMaxUsageError)
                      : qsTr("额度查询：%1").arg(root.miniMaxUsageStatus)))
            type: root.isCodexZh
                ? (root.codexzhUsageError.length > 0
                   ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive)
                : (root.isDeepSeek
                   ? (root.deepseekUsageError.length > 0
                      ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive)
                   : (root.miniMaxUsageError.length > 0
                      ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive))
        }

        SectionHeading {
            visible: root.isCustom
            text: qsTr("限额项")
        }

        Column {
            objectName: "customQuotaSection"
            visible: root.isCustom
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            Repeater {
                model: root.quotaModel

                delegate: ColumnLayout {
                    id: quotaColumn

                    required property int index
                    required property var modelData

                    width: parent.width
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: root.formWidth
                        Layout.maximumWidth: root.formWidth

                        Kirigami.Heading {
                            Layout.fillWidth: true
                            level: 4
                            text: qsTr("限额 %1").arg(quotaColumn.index + 1)
                        }

                        QQC2.ToolButton {
                            icon.name: "edit-delete"
                            enabled: (root.candidate.plans || []).length > 1
                            Accessible.name: qsTr("删除限额 %1").arg(quotaColumn.index + 1)
                            QQC2.ToolTip.text: Accessible.name
                            QQC2.ToolTip.visible: hovered
                            onClicked: root.removePlan(quotaColumn.index)
                        }
                    }

                    GridLayout {
                        objectName: "quotaForm"
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: root.formWidth
                        Layout.maximumWidth: root.formWidth
                        columns: 2
                        columnSpacing: Kirigami.Units.largeSpacing
                        rowSpacing: Kirigami.Units.smallSpacing

                        FieldLabel { text: qsTr("限额名称：") }

                        QQC2.TextField {
                            objectName: "quotaNameField"
                            Layout.preferredWidth: root.fieldWidth
                            Layout.maximumWidth: root.fieldWidth
                            text: quotaColumn.modelData.planName || ""
                            placeholderText: qsTr("例如 5 小时、每周或每月")
                            onTextEdited: root.updatePlan(quotaColumn.index, "planName", text)
                        }

                        FieldLabel { text: qsTr("单位：") }

                        QQC2.TextField {
                            Layout.preferredWidth: root.fieldWidth
                            Layout.maximumWidth: root.fieldWidth
                            text: quotaColumn.modelData.unit || ""
                            placeholderText: qsTr("例如 次、Token、元或 %")
                            onTextEdited: root.updatePlan(quotaColumn.index, "unit", text)
                        }

                        FieldLabel { text: qsTr("已用量变量：") }

                        QQC2.TextField {
                            Layout.preferredWidth: root.fieldWidth
                            Layout.maximumWidth: root.fieldWidth
                            text: quotaColumn.modelData.usedVariable || ""
                            placeholderText: "${used}"
                            onTextEdited: root.updatePlan(quotaColumn.index, "usedVariable", text)
                        }

                        FieldLabel { text: qsTr("限额总量变量：") }

                        QQC2.TextField {
                            Layout.preferredWidth: root.fieldWidth
                            Layout.maximumWidth: root.fieldWidth
                            text: quotaColumn.modelData.limitVariable || ""
                            placeholderText: "${limit}"
                            onTextEdited: root.updatePlan(quotaColumn.index, "limitVariable", text)
                        }

                        FieldLabel { text: qsTr("到期时间变量：") }

                        QQC2.TextField {
                            objectName: "resetVariableField"
                            Layout.preferredWidth: root.fieldWidth
                            Layout.maximumWidth: root.fieldWidth
                            text: quotaColumn.modelData.resetVariable || ""
                            placeholderText: "${resetAt}"
                            onTextEdited: root.updatePlan(quotaColumn.index, "resetVariable", text)
                        }
                    }
                }
            }

            QQC2.Button {
                x: Math.max(0, (parent.width - root.formWidth) / 2)
                text: qsTr("添加限额")
                icon.name: "list-add"
                onClicked: root.addPlan()
            }
        }

        SectionHeading {
            visible: root.isCustom
            text: qsTr("JavaScript 查询脚本")
        }

        ColumnLayout {
            objectName: "scriptEditorSection"
            visible: root.isCustom
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            RowLayout {
                Layout.fillWidth: true

                QQC2.ComboBox {
                    id: completionBox

                    objectName: "scriptCompletionBox"
                    Layout.fillWidth: true
                    model: ["request", "extractor", "response", "used", "limit", "resetAt"]
                    Accessible.name: qsTr("代码提示")
                }

                QQC2.Button {
                    text: qsTr("插入提示")
                    icon.name: "insert-text"
                    onClicked: {
                        scriptArea.insert(scriptArea.cursorPosition, completionBox.currentText)
                        scriptArea.forceActiveFocus()
                    }
                }

                QQC2.Button {
                    id: wrapButton

                    objectName: "scriptWrapButton"
                    text: qsTr("自动换行")
                    icon.name: "format-text-wrap"
                    checkable: true
                }

                QQC2.Button {
                    objectName: "formatScriptButton"
                    text: qsTr("格式化")
                    icon.name: "format-indent-more"
                    onClicked: root.formatScript()
                }

                QQC2.Button {
                    objectName: "testScriptButton"
                    text: qsTr("测试脚本")
                    icon.name: "media-playback-start"
                    onClicked: root.testScriptContract()
                }
            }

            QQC2.Frame {
                id: scriptFrame

                objectName: "scriptEditorFrame"
                Layout.fillWidth: true
                Layout.preferredHeight: root.scriptEditorHeight

                QQC2.ScrollView {
                    id: editorScroll

                    anchors.fill: parent
                    clip: true

                    RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        QQC2.Label {
                            id: lineNumbers

                            objectName: "scriptLineNumbers"
                            Layout.alignment: Qt.AlignTop
                            text: ScriptTools.lineNumbers(scriptArea.text)
                            color: Kirigami.Theme.disabledTextColor
                            horizontalAlignment: Text.AlignRight
                            font.family: "monospace"
                        }

                        Kirigami.Separator {
                            Layout.fillHeight: true
                        }

                        QQC2.TextArea {
                            id: scriptArea

                            objectName: "scriptEditor"
                            Layout.preferredWidth: wrapButton.checked
                                ? Math.max(Kirigami.Units.gridUnit * 20,
                                           editorScroll.availableWidth
                                           - lineNumbers.implicitWidth
                                           - Kirigami.Units.smallSpacing * 3)
                                : Math.max(Kirigami.Units.gridUnit * 28, implicitWidth)
                            Layout.preferredHeight: Math.max(Kirigami.Units.gridUnit * 14,
                                                             contentHeight)
                            text: root.candidate.script || ""
                            wrapMode: wrapButton.checked ? TextEdit.Wrap : TextEdit.NoWrap
                            selectByKeyboard: true
                            selectByMouse: true
                            font.family: "monospace"
                            background: null
                            onTextChanged: {
                                if (activeFocus && text !== (root.candidate.script || ""))
                                    root.updateField("script", text)
                            }
                        }
                    }
                }

                MouseArea {
                    id: resizeHandle

                    objectName: "scriptResizeHandle"
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    width: Kirigami.Units.gridUnit * 1.5
                    height: width
                    z: 2
                    hoverEnabled: true
                    cursorShape: Qt.SizeFDiagCursor
                    Accessible.name: qsTr("拖动调整代码编辑区高度")
                    property real dragStartY: 0
                    property real dragStartHeight: 0

                    onPressed: mouse => {
                        dragStartY = mapToItem(root, mouse.x, mouse.y).y
                        dragStartHeight = root.scriptEditorHeight
                    }
                    onPositionChanged: mouse => {
                        if (!pressed)
                            return
                        const currentY = mapToItem(root, mouse.x, mouse.y).y
                        root.setScriptEditorHeight(dragStartHeight + currentY - dragStartY)
                    }

                    QQC2.Label {
                        anchors.centerIn: parent
                        text: "◢"
                        color: Kirigami.Theme.disabledTextColor
                    }

                    QQC2.ToolTip.visible: containsMouse
                    QQC2.ToolTip.text: Accessible.name
                }
            }

            QQC2.Label {
                Layout.fillWidth: true
                text: qsTr("脚本沿用 cc-switch 的 request/extractor 结构；查询在独立进程中执行，不会阻塞 Plasma。")
                color: Kirigami.Theme.disabledTextColor
                wrapMode: Text.Wrap
            }

            Kirigami.InlineMessage {
                visible: root.scriptTestMessage.length > 0
                Layout.fillWidth: true
                text: root.scriptTestMessage
                type: root.scriptTestError
                    ? Kirigami.MessageType.Error : Kirigami.MessageType.Positive
            }
        }

        Kirigami.InlineMessage {
            visible: !root.validation.valid
            Layout.fillWidth: true
            text: root.validation.message
            type: Kirigami.MessageType.Error
        }
    }

    Component.onCompleted: attachHighlighter()
}
