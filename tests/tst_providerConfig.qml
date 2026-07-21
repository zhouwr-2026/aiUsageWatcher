import QtQuick
import QtTest
import "../package/contents/js/providerConfig.js" as ProviderConfig

Item {
    id: host

    width: 640
    height: 640

    readonly property url providersConfigUrl: Qt.resolvedUrl(
        "../package/contents/ui/config/ProvidersConfig.qml")

    TestCase {
        name: "ProviderConfig"
        when: windowShown

        function provider(id, name) {
            return {
                id: id,
                providerName: name || "Provider " + id,
                sourceLabel: "自定义",
                trustMode: "strict",
                template: "%1 限额  %2/%3  重置于 %4",
                plans: [{ id: "plan-1", planName: "5小时", unit: "次" }]
            }
        }

        function createPage(definitions) {
            const component = Qt.createComponent(host.providersConfigUrl)
            compare(component.status, Component.Ready, component.errorString())
            const page = component.createObject(host, {
                cfg_providers: JSON.stringify(definitions)
            })
            verify(page !== null, component.errorString())
            return page
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

        function test_rejects_zero_plans() {
            const candidate = provider("alpha", "Alpha")
            candidate.plans = []
            verify(!ProviderConfig.validateProvider(candidate, []).valid)
        }

        function test_rejects_empty_or_duplicate_plan_fields() {
            const emptyId = provider("alpha", "Alpha")
            emptyId.plans[0].id = " "
            verify(!ProviderConfig.validateProvider(emptyId, []).valid)

            const emptyName = provider("alpha", "Alpha")
            emptyName.plans[0].planName = " "
            verify(!ProviderConfig.validateProvider(emptyName, []).valid)

            const duplicateName = provider("alpha", "Alpha")
            duplicateName.plans.push({ id: "plan-2", planName: "5小时", unit: "" })
            verify(!ProviderConfig.validateProvider(duplicateName, []).valid)

            const duplicateId = provider("alpha", "Alpha")
            duplicateId.plans.push({ id: "plan-1", planName: "7天", unit: "" })
            verify(!ProviderConfig.validateProvider(duplicateId, []).valid)
        }

        function test_rejects_each_missing_template_placeholder() {
            const placeholders = ["%1", "%2", "%3", "%4"]
            for (let i = 0; i < placeholders.length; ++i) {
                const candidate = provider("alpha", "Alpha")
                candidate.template = candidate.template.replace(placeholders[i], "")
                verify(!ProviderConfig.validateProvider(candidate, []).valid,
                       "Missing " + placeholders[i] + " must be invalid")
            }
        }

        function test_accepts_valid_provider_and_renders_exact_preview() {
            verify(ProviderConfig.validateProvider(provider("alpha", "Alpha"), []).valid)
            compare(ProviderConfig.previewTemplate("%1 限额  %2/%3  重置于 %4"),
                    "5小时 限额  65/100  重置于 今天 18:00")
        }

        function test_parse_and_serialize_keep_only_definition_fields() {
            const legacy = provider("alpha", "Alpha")
            legacy.usedPercent = 65
            legacy.plans[0].usedText = "65"
            const parsed = ProviderConfig.parseWorkingDefinitions(JSON.stringify([legacy]))

            compare(parsed.length, 1)
            compare(parsed[0].id, "alpha")
            verify(parsed[0].usedPercent === undefined)
            verify(parsed[0].plans[0].usedText === undefined)
            compare(ProviderConfig.parseWorkingDefinitions(
                        ProviderConfig.serializeDefinitions(parsed))[0].providerName, "Alpha")
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

        function test_cancel_does_not_touch_cfg_value() {
            const original = provider("alpha", "Alpha")
            const page = createPage([original])
            const before = page.cfg_providers

            verify(page.beginEdit("alpha"))
            compare(page.editorVisible, true)
            const editor = findChild(page, "providerEditor")
            editor.updateField("providerName", "Unsaved edit")
            compare(editor.currentCandidate().providerName, "Unsaved edit")
            page.cancelEditor()

            compare(page.editorVisible, false)
            compare(page.cfg_providers, before)
            compare(page.workingCount, 1)
            page.destroy()
        }

        function test_minimax_editor_has_masked_api_key_field() {
            const page = createPage([provider("minimax", "MiniMax")])

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
    }
}
