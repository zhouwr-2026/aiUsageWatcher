import QtQuick
import QtTest
import "../package/contents/js/providerConfig.js" as ProviderConfig
import "../package/contents/js/providerCatalog.js" as ProviderCatalog
import "../package/contents/js/scriptTools.js" as ScriptTools

Item {
    id: host

    width: 640
    height: 640

    readonly property url providersConfigUrl: Qt.resolvedUrl(
        "../package/contents/ui/config/ProvidersConfig.qml")

    QtObject {
        id: sharedBackendMock

        property string sharedProviders: ""
        property var codexAccounts: []
        property int saveCalls: 0
        property string savedProviders: ""

        function saveSharedProviders(providers) {
            ++saveCalls
            savedProviders = providers
            sharedProviders = providers
            return true
        }
    }

    TestCase {
        name: "ProviderConfig"
        when: windowShown

        function provider(id, name) {
            return {
                catalogId: "custom",
                id: id,
                providerName: name || "Provider " + id,
                website: "https://example.com/",
                vendor: "自定义",
                sourceLabel: "自定义",
                trustMode: "strict",
                template: "%1 限额  %2/%3  重置于 %4",
                script: ScriptTools.DEFAULT_SCRIPT,
                plans: [{
                    id: "plan-1",
                    planName: "5小时",
                    unit: "次",
                    sourceType: "http-js",
                    usedVariable: "${used}",
                    limitVariable: "${limit}"
                }]
            }
        }

        function createPage(definitions, sharedProviders) {
            sharedBackendMock.sharedProviders = sharedProviders || ""
            sharedBackendMock.codexAccounts = []
            sharedBackendMock.saveCalls = 0
            sharedBackendMock.savedProviders = ""
            const component = Qt.createComponent(host.providersConfigUrl)
            compare(component.status, Component.Ready, component.errorString())
            const page = component.createObject(host, {
                cfg_providers: JSON.stringify(definitions),
                backendOverride: sharedBackendMock
            })
            verify(page !== null, component.errorString())
            return page
        }

        function test_shared_providers_override_local_and_are_saved() {
            const page = createPage(
                [provider("local", "Local")],
                JSON.stringify([provider("shared", "Shared")]))

            compare(page.workingCount, 1)
            verify(page.beginEdit("shared"))
            verify(page.saveConfig())
            compare(sharedBackendMock.saveCalls, 1)
            compare(ProviderConfig.parseWorkingDefinitions(
                        sharedBackendMock.savedProviders)[0].providerName, "Shared")
            page.destroy()
        }

        function test_rejects_empty_provider_name() {
            const candidate = provider("alpha", "Alpha")
            candidate.providerName = "  "
            verify(!ProviderConfig.validateProvider(candidate, []).valid)
        }

        function test_rejects_duplicate_stable_provider_id() {
            const candidate = provider("alpha", "Alpha renamed")
            verify(!ProviderConfig.validateProvider(candidate,
                                                     [provider("alpha", "Alpha")]).valid)
        }

        function test_catalog_has_requested_fixed_providers_and_custom_last() {
            const options = ProviderCatalog.providerOptions()
            compare(options.length, 12)
            compare(options[0].text, "Codex")
            compare(options[1].text, "Claude Code")
            compare(options[2].text, "OpenCode Go")
            compare(options[3].text, "MiniMax")
            compare(options[4].text, "智谱 GLM")
            compare(options[5].text, "Kimi For Coding")
            compare(options[6].text, "硅基流动")
            compare(options[7].text, "CodexZH")
            compare(options[8].text, "DeepSeek")
            compare(options[9].text, "Agnes AI")
            compare(options[10].text, "Command Code")
            compare(options[11].value, "custom")
        }

        function test_fixed_provider_definition_is_canonical() {
            const miniMax = ProviderCatalog.definitionFor("minimax")
            compare(miniMax.id, "minimax")
            compare(miniMax.providerName, "MiniMax")
            compare(miniMax.website, "https://www.minimaxi.com/")
            verify(miniMax.plans.length > 0)
            compare(miniMax.plans[0].sourceType, "native")
            miniMax.plans = []
            verify(!ProviderConfig.validateProvider(miniMax, []).valid)
        }

        function test_rejects_zero_quotas() {
            const candidate = provider("alpha", "Alpha")
            candidate.plans = []
            verify(!ProviderConfig.validateProvider(candidate, []).valid)
        }

        function test_rejects_empty_or_duplicate_quota_names() {
            const emptyName = provider("alpha", "Alpha")
            emptyName.plans[0].planName = " "
            verify(!ProviderConfig.validateProvider(emptyName, []).valid)

            const duplicateName = provider("alpha", "Alpha")
            duplicateName.plans.push({ id: "plan-2", planName: "5小时", unit: "" })
            verify(!ProviderConfig.validateProvider(duplicateName, []).valid)
        }

        function test_rejects_invalid_website_and_variable_names() {
            const invalidWebsite = provider("alpha", "Alpha")
            invalidWebsite.website = "not-a-url"
            verify(!ProviderConfig.validateProvider(invalidWebsite, []).valid)

            const invalidUsed = provider("alpha", "Alpha")
            invalidUsed.plans[0].usedVariable = "used"
            verify(!ProviderConfig.validateProvider(invalidUsed, []).valid)

            const invalidLimit = provider("alpha", "Alpha")
            invalidLimit.plans[0].limitVariable = "${123}"
            verify(!ProviderConfig.validateProvider(invalidLimit, []).valid)

            const invalidReset = provider("alpha", "Alpha")
            invalidReset.plans[0].resetVariable = "resetAt"
            verify(!ProviderConfig.validateProvider(invalidReset, []).valid)
        }

        function test_rejects_script_without_request_or_extractor_contract() {
            const candidate = provider("alpha", "Alpha")
            candidate.script = "return { used: 1, limit: 2 }"
            verify(!ProviderConfig.validateProvider(candidate, []).valid)
        }

        function test_new_provider_is_custom_with_one_quota() {
            const page = createPage([])
            page.beginAdd()
            const editor = findChild(page, "providerEditor")
            const candidate = editor.currentCandidate()

            compare(candidate.catalogId, "custom")
            compare(candidate.plans.length, 1)
            compare(candidate.plans[0].sourceType, "http-js")
            compare(candidate.plans[0].usedVariable, "${used}")
            page.destroy()
        }

        function test_new_provider_survives_page_recreation() {
            const page = createPage([provider("alpha", "Alpha")])
            page.beginAdd()
            const editor = findChild(page, "providerEditor")
            editor.updateField("id", "beta")
            editor.updateField("providerName", "Beta")
            editor.updateField("website", "https://beta.example.com/")
            editor.updatePlan(0, "planName", "月额度")
            wait(0)

            verify(editor.validation.valid, editor.validation.message)
            compare(page.workingCount, 2)
            const savedValue = page.cfg_providers
            page.destroy()

            const reopenedPage = createPage(
                ProviderConfig.parseWorkingDefinitions(savedValue))
            compare(reopenedPage.workingCount, 2)
            verify(reopenedPage.beginEdit("beta"))
            compare(findChild(reopenedPage, "providerEditor")
                    .currentCandidate().providerName, "Beta")
            reopenedPage.destroy()
        }

        function test_outer_kcm_save_commits_current_valid_candidate() {
            const page = createPage([provider("alpha", "Alpha")])
            page.beginAdd()
            const editor = findChild(page, "providerEditor")

            page.editorVisible = false
            editor.updateField("id", "beta")
            editor.updateField("providerName", "Beta")
            editor.updateField("website", "https://beta.example.com/")
            editor.updatePlan(0, "planName", "月额度")
            page.editorVisible = true
            compare(page.workingCount, 1)

            verify(page.saveConfig())
            compare(page.workingCount, 2)
            compare(ProviderConfig.parseWorkingDefinitions(
                        page.cfg_providers)[1].id, "beta")
            page.destroy()
        }

        function test_accepts_valid_custom_and_fixed_provider() {
            verify(ProviderConfig.validateProvider(provider("alpha", "Alpha"), []).valid)
            verify(ProviderConfig.validateProvider(
                       ProviderCatalog.definitionFor("minimax"), []).valid)
        }

        function test_fixed_provider_from_config_with_price_validates() {
            // 模拟配置加载形态：preset + 用户可配置字段（enabled/logoPath/price），
            // 键顺序与预设一致。此场景此前因未剔除 enabled 导致校验失败、保存被拒。
            const configured = JSON.parse(JSON.stringify(ProviderCatalog.definitionFor("minimax")))
            configured.enabled = true
            configured.price = 30
            configured.logoPath = "file:///custom/logo.png"
            verify(ProviderConfig.validateProvider(configured, []).valid)
        }

        function test_parse_and_serialize_keep_only_definition_fields() {
            const legacy = provider("alpha", "Alpha")
            legacy.usedPercent = 65
            legacy.plans[0].usedText = "65"
            const parsed = ProviderConfig.parseWorkingDefinitions(JSON.stringify([legacy]))

            compare(parsed.length, 1)
            compare(parsed[0].id, "alpha")
            compare(parsed[0].catalogId, "custom")
            verify(parsed[0].usedPercent === undefined)
            verify(parsed[0].plans[0].usedText === undefined)
            compare(ProviderConfig.parseWorkingDefinitions(
                    ProviderConfig.serializeDefinitions(parsed))[0].providerName, "Alpha")
        }

        function test_legacy_definition_migrates_without_losing_manual_values() {
            const legacy = {
                id: "legacy-custom",
                providerName: "旧自定义",
                vendor: "自定义",
                sourceLabel: "自定义",
                trustMode: "strict",
                template: "%1 限额  %2/%3  重置于 %4",
                plans: [{
                    id: "5h",
                    planName: "5 小时",
                    unit: "次",
                    sourceType: "manual",
                    manualUsed: 12,
                    limit: 100
                }]
            }
            const migrated = ProviderConfig.parseWorkingDefinitions(JSON.stringify([legacy]))[0]
            compare(migrated.catalogId, "custom")
            compare(migrated.plans[0].manualUsed, 12)
            compare(migrated.plans[0].limit, 100)
            compare(migrated.plans[0].sourceType, "manual")
            compare(migrated.plans[0].usedVariable, "${used}")
            verify(migrated.script.length > 0)
        }

        function test_page_crud_updates_only_cfg_working_value() {
            const original = provider("alpha", "Alpha")
            const persisted = JSON.stringify([original])
            const page = createPage([original])

            verify(findChild(page, "providerEditorPage") !== null)
            compare(page.workingCount, 1)

            verify(page.addProvider(provider("beta", "Beta")))
            compare(ProviderConfig.parseWorkingDefinitions(page.cfg_providers).length, 2)

            const edited = provider("alpha", "Alpha edited")
            verify(page.updateProvider(edited))
            compare(ProviderConfig.parseWorkingDefinitions(page.cfg_providers)[0].providerName,
                    "Alpha edited")

            verify(page.deleteProvider("beta"))
            compare(ProviderConfig.parseWorkingDefinitions(page.cfg_providers).length, 1)
            compare(persisted, JSON.stringify([original]))
            page.destroy()
        }

        function test_editor_updates_outer_kcm_working_value() {
            const original = provider("alpha", "Alpha")
            const page = createPage([original])

            verify(page.beginEdit("alpha"))
            compare(page.editorVisible, true)
            const editor = findChild(page, "providerEditor")
            editor.updateField("providerName", "Alpha edited")
            compare(ProviderConfig.parseWorkingDefinitions(
                        page.cfg_providers)[0].providerName, "Alpha edited")
            page.cancelEditor()

            compare(page.editorVisible, false)
            compare(page.workingCount, 1)
            page.destroy()
        }

        function test_invalid_editor_value_does_not_replace_working_value() {
            const page = createPage([provider("alpha", "Alpha")])
            const before = page.cfg_providers

            verify(page.beginEdit("alpha"))
            findChild(page, "providerEditor").updateField("providerName", " ")
            compare(page.cfg_providers, before)
            page.destroy()
        }

        function test_editor_uses_one_navigation_header_and_no_inner_buttons() {
            const page = createPage([provider("alpha", "Alpha")])
            const titleItem = page.titleDelegate.createObject(host)
            verify(titleItem !== null)
            const title = findChild(titleItem, "providerPageTitle")
            const back = findChild(titleItem, "providerBackButton")
            const add = findChild(page, "addProviderButton")

            verify(findChild(page, "providerNavigationHeader") === null)
            compare(title.text, "供应商")
            compare(back.visible, false)
            compare(add.visible, true)
            verify(findChild(page, "saveProviderButton") === null)

            verify(page.beginEdit("alpha"))
            compare(title.text, "编辑 Alpha")
            compare(back.visible, true)
            compare(back.text, "")
            titleItem.destroy()
            page.destroy()
        }

        function test_provider_stack_uses_current_page_height() {
            const page = createPage([provider("alpha", "Alpha")])
            const stack = findChild(page, "providerStackLayout")
            const listPage = findChild(page, "providerListPage")
            const editorPage = findChild(page, "providerEditorPage")

            compare(stack.implicitHeight, listPage.implicitHeight)
            compare(page.verticalScrollBarPolicy, 0)
            verify(page.beginEdit("alpha"))
            compare(stack.implicitHeight, editorPage.implicitHeight)
            page.flickable.contentY = 20
            page.cancelEditor()
            wait(0)
            compare(stack.implicitHeight, listPage.implicitHeight)
            compare(page.flickable.contentHeight, listPage.implicitHeight + 6)
            compare(page.flickable.contentY, 0)
            page.destroy()
        }

        function test_form_fields_are_bounded_and_aligned() {
            const page = createPage([provider("alpha", "Alpha")])
            page.width = 1200
            page.height = 1000
            verify(page.beginEdit("alpha"))
            wait(0)

            const editor = findChild(page, "providerEditor")
            const providerId = findChild(editor, "providerIdField")
            const quotaName = findChild(editor, "quotaNameField")
            const form = findChild(editor, "providerForm")
            const scriptSection = findChild(editor, "scriptEditorSection")
            const resetVariable = findChild(editor, "resetVariableField")
            verify(providerId !== null)
            verify(quotaName !== null)
            verify(resetVariable !== null)
            verify(form["wideMode"] === undefined)
            verify(providerId.width <= 420)
            verify(quotaName.width <= 420)
            const providerX = providerId.mapToItem(editor, 0, 0).x
            const quotaX = quotaName.mapToItem(editor, 0, 0).x
            verify(providerX >= 0)
            verify(quotaX >= 0)
            verify(providerX + providerId.width <= editor.width,
                   "provider=" + providerX + "+" + providerId.width + " editor=" + editor.width)
            verify(quotaX + quotaName.width <= editor.width,
                   "quota=" + quotaX + "+" + quotaName.width + " editor=" + editor.width)
            verify(Math.abs(providerX - quotaX) <= 24,
                   "providerX=" + providerX + " quotaX=" + quotaX
                   + " editor=" + editor.width)
            verify(scriptSection.width >= editor.width - 48,
                   "script=" + scriptSection.width + " editor=" + editor.width)
            page.destroy()
        }

        function test_quota_field_keeps_focus_while_editing() {
            const page = createPage([provider("alpha", "Alpha")])
            verify(page.beginEdit("alpha"))
            wait(0)

            const editor = findChild(page, "providerEditor")
            let quotaName = findChild(editor, "quotaNameField")
            quotaName.forceActiveFocus()
            verify(quotaName.activeFocus)

            quotaName.text += "a"
            quotaName.textEdited()
            wait(0)

            quotaName = findChild(editor, "quotaNameField")
            verify(quotaName.activeFocus)
            compare(quotaName.text, "5小时a")
            page.destroy()
        }

        function test_dynamic_quota_forms_do_not_register_as_form_twins() {
            const page = createPage([provider("alpha", "Alpha")])
            verify(page.beginEdit("alpha"))
            wait(0)

            const editor = findChild(page, "providerEditor")
            const quotaForm = findChild(editor, "quotaForm")
            verify(quotaForm !== null)
            verify(quotaForm["twinFormLayouts"] === undefined)

            for (let i = 0; i < 20; ++i) {
                editor.selectCatalog("minimax")
                wait(0)
                editor.selectCatalog("custom")
                wait(0)
            }
            page.destroy()
            wait(0)
        }

        function test_provider_choice_autofills_and_hides_custom_fields() {
            const page = createPage([])
            page.beginAdd()
            const editor = findChild(page, "providerEditor")
            editor.selectCatalog("minimax")

            const candidate = editor.currentCandidate()
            compare(candidate.id, "minimax")
            compare(candidate.providerName, "MiniMax")
            compare(candidate.website, "https://www.minimaxi.com/")
            compare(findChild(editor, "customQuotaSection").visible, false)
            compare(findChild(editor, "scriptEditorSection").visible, false)
            page.destroy()
        }

        function test_custom_editor_has_line_numbers_format_and_contract_test() {
            const page = createPage([provider("alpha", "Alpha")])
            verify(page.beginEdit("alpha"))
            const editor = findChild(page, "providerEditor")
            const completion = findChild(editor, "scriptCompletionBox")
            const scriptArea = findChild(editor, "scriptEditor")
            const wrapButton = findChild(editor, "scriptWrapButton")
            verify(findChild(editor, "scriptLineNumbers") !== null)
            verify(findChild(editor, "formatScriptButton") !== null)
            verify(findChild(editor, "testScriptButton") !== null)
            verify(findChild(editor, "scriptResizeHandle") !== null)
            compare(completion.model[3], "used")
            compare(completion.model[4], "limit")
            compare(completion.model[5], "resetAt")
            compare(scriptArea.wrapMode, TextEdit.NoWrap)
            wrapButton.checked = true
            compare(scriptArea.wrapMode, TextEdit.Wrap)

            const initialHeight = editor.scriptEditorHeight
            editor.setScriptEditorHeight(initialHeight + 100)
            compare(editor.scriptEditorHeight, initialHeight + 100)
            editor.setScriptEditorHeight(0)
            compare(editor.scriptEditorHeight, editor.minimumScriptEditorHeight)
            editor.testScriptContract()
            verify(editor.scriptTestMessage.indexOf("契约验证通过") >= 0)
            verify(editor.scriptTestMessage.indexOf("刷新将执行真实查询") >= 0)
            page.destroy()
        }

        function test_provider_order_can_be_changed() {
            const page = createPage([provider("alpha", "Alpha"), provider("beta", "Beta")])

            verify(page.moveProvider("beta", -1))
            const definitions = ProviderConfig.parseWorkingDefinitions(page.cfg_providers)
            compare(definitions[0].id, "beta")
            compare(definitions[1].id, "alpha")
            compare(page.cfg_customOrder, JSON.stringify(["beta", "alpha"]))
            verify(!page.moveProvider("beta", -1))
            page.destroy()
        }

        function test_minimax_editor_has_masked_api_key_field() {
            const page = createPage([ProviderCatalog.definitionFor("minimax")])

            verify(page.beginEdit("minimax"))
            const field = findChild(page, "miniMaxApiKeyField")
            verify(field !== null)
            verify(field.visible)
            compare(field.echoMode, TextInput.Password)
            verify(findChild(page, "miniMaxCredentialMessage").text.indexOf("KDE 钱包") >= 0)

            field.text = "secret-must-not-enter-config"
            const saveButton = findChild(page, "saveMiniMaxApiKeyButton")
            verify(saveButton.enabled)
            saveButton.clicked()
            compare(field.text, "")
            verify(page.cfg_providers.indexOf("secret-must-not-enter-config") < 0)
            page.cancelEditor()
            page.destroy()
        }

        function test_codexzh_clear_does_not_clear_minimax_key() {
            const page = createPage([
                ProviderCatalog.definitionFor("minimax"),
                ProviderCatalog.definitionFor("codexzh")
            ])

            compare(page.credentialBackendMethod("codexzh", "clear"),
                    "clearCodexZhApiKey")
            compare(page.credentialBackendMethod("minimax", "clear"),
                    "clearMiniMaxApiKey")
            compare(page.credentialBackendMethod("agnes-ai", "save"),
                    "saveAgnesApiKey")
            compare(page.credentialBackendMethod("agnes-ai", "refresh"),
                    "refreshAgnesUsage")
            compare(page.credentialBackendMethod("command-code", "save"),
                    "saveCommandCodeCookie")
            compare(page.credentialBackendMethod("command-code", "clear"),
                    "clearCommandCodeCookie")
            compare(page.credentialBackendMethod("command-code", "refresh"),
                    "refreshCommandCodeUsage")
            page.destroy()
        }

        function test_agnes_and_command_code_editor_fields() {
            const page = createPage([
                ProviderCatalog.definitionFor("agnes-ai"),
                ProviderCatalog.definitionFor("command-code")
            ])

            // Agnes AI：API Key 输入框（复用 API Key 区）
            verify(page.beginEdit("agnes-ai"))
            const agnesField = findChild(page, "agnesApiKeyField")
            verify(agnesField !== null)
            verify(agnesField.visible)
            compare(agnesField.echoMode, TextInput.Password)

            // Command Code：Cookie 输入框（专用凭据区）
            verify(page.beginEdit("command-code"))
            const cookieField = findChild(page, "commandCodeCookieField")
            verify(cookieField !== null)
            verify(cookieField.visible)
            verify(findChild(page, "saveCommandCodeCredentialButton") !== null)
            page.cancelEditor()
            page.destroy()
        }

        function test_codex_editor_uses_browser_device_login() {
            const page = createPage([ProviderCatalog.definitionFor("codex")])

            verify(page.beginEdit("codex"))
            const editor = findChild(page, "providerEditor")
            const loginButton = findChild(editor, "startCodexLoginButton")
            const browserButton = findChild(editor, "openCodexLoginPageButton")
            const codeField = findChild(editor, "codexDeviceCodeField")
            verify(loginButton.visible)
            verify(loginButton.enabled)
            verify(!browserButton.visible)
            verify(!codeField.visible)

            editor.codexAccounts = [{
                profileId: "profile-1234",
                accountId: "account-123",
                login: "user@example.com",
                isDefault: true
            }]
            wait(0)
            compare(loginButton.text, "添加其他账号")
            verify(findChild(editor, "codexAccountRow") !== null)
            verify(findChild(editor, "removeCodexAccountButton") !== null)

            editor.codexLoginBusy = true
            editor.codexDeviceCode = "K58J-YY9PL"
            wait(0)
            verify(browserButton.visible)
            verify(browserButton.enabled)
            verify(codeField.visible)
            compare(codeField.text, "K58J-YY9PL")
            page.destroy()
        }

        function test_codex_accounts_accept_qvariantlist_like_values() {
            const page = createPage([ProviderCatalog.definitionFor("codex")])
            sharedBackendMock.codexAccounts = {
                0: {
                    profileId: "profile-1234",
                    accountId: "account-123",
                    login: "user@example.com",
                    isDefault: true
                },
                length: 1
            }

            page.syncCodexLoginState()
            compare(page.codexAccounts.length, 1)
            verify(page.beginEdit("codex"))
            verify(findChild(page, "codexAccountRow") !== null)
            verify(findChild(page, "removeCodexAccountButton") !== null)
            page.destroy()
        }
    }
}
