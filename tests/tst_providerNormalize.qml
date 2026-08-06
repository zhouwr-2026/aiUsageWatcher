import QtQuick
import QtTest
import "../package/contents/js/displayProvider.js" as DisplayProvider
import "../package/contents/js/providerCatalog.js" as ProviderCatalog
import "../package/contents/js/providerNormalize.js" as ProviderNormalize

TestCase {
    name: "ProviderNormalize"

    function test_uninitialized_config_uses_builtin_providers() {
        const definitions = ProviderNormalize.normalizeDefinitions("")

        verify(definitions.length > 0)
        compare(definitions[0].plans[0].sourceType, "native")
    }

    function test_explicit_empty_config_stays_empty() {
        compare(ProviderNormalize.normalizeDefinitions("[]").length, 0)
    }

    function test_disabled_definition_stays_in_configuration_model() {
        const definition = ProviderCatalog.definitionFor("minimax")
        definition.enabled = false

        const definitions = ProviderNormalize.normalizeDefinitions([definition])

        compare(definitions.length, 1)
        compare(definitions[0].id, "minimax")
        compare(definitions[0].enabled, false)
        compare(DisplayProvider.buildDisplay(definitions, []).length, 0)
    }

    function test_cpp_array_like_plans_become_javascript_array() {
        const snapshots = ProviderNormalize.replaceSnapshot([], {
            providerId: "codex",
            statusLabel: "可用",
            plans: {
                0: { planId: "five-hours", used: 67, total: 100 },
                length: 1
            }
        })

        verify(Array.isArray(snapshots[0].plans))
        compare(snapshots[0].plans.length, 1)
        compare(snapshots[0].plans[0].used, 67)
    }
}
