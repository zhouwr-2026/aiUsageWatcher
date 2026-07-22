import QtQuick 2.15
import QtTest 1.3
import "../package/contents/js/mockData.js" as MockData

TestCase {
    name: "MockDataContract"

    function providerById(providers, id) {
        for (var i = 0; i < providers.length; ++i) {
            if (providers[i].id === id)
                return providers[i]
        }
        return null
    }

    function test_thresholds() {
        compare(MockData.usageClass(84, "bar"), "bar-green")
        compare(MockData.usageClass(85, "bar"), "bar-yellow")
        compare(MockData.usageClass(94, "bar"), "bar-yellow")
        compare(MockData.usageClass(95, "bar"), "bar-red")
        compare(MockData.usageClass(-1, "bar"), "bar-gray")
    }

    function test_tightest_usage_uses_largest_valid_percent() {
        var tightest = MockData.tightestUsage([{
            providerName: "A",
            plans: [{ planName: "low", usedPercent: 22 }]
        }, {
            providerName: "B",
            plans: [{ planName: "high", usedPercent: 88 }]
        }, {
            providerName: "C",
            plans: [{ planName: "mid", usedPercent: 67 }]
        }])

        compare(tightest.usedPercent, 88)
        compare(tightest.providerName, "B")
        compare(tightest.planName, "high")
    }

    function test_provider_rotation_keeps_provider_and_plan_in_sync() {
        var providers = [{
            providerName: "MiniMax",
            plans: [
                { planName: "5 小时", usedPercent: 12 },
                { planName: "每周", usedPercent: 28 }
            ]
        }, {
            providerName: "Codex",
            plans: [{ planName: "每周", usedPercent: 67 }]
        }, {
            providerName: "未配置",
            plans: []
        }]

        var first = MockData.providerUsageAt(providers, 0)
        var second = MockData.providerUsageAt(providers, 1)
        var wrapped = MockData.providerUsageAt(providers, 4)
        var empty = MockData.providerUsageAt(providers, 2)

        compare(first.providerName, "MiniMax")
        compare(first.planName, "每周")
        compare(first.usedPercent, 28)
        compare(second.providerName, "Codex")
        compare(second.usedPercent, 67)
        compare(wrapped.providerName, "Codex")
        compare(empty.providerName, "未配置")
        compare(empty.usedPercent, -1)
        compare(empty.statusLabel, "")
    }

    function test_provider_rotation_includes_providers_without_usage() {
        var providers = [{
            providerName: "MiniMax",
            plans: [{ planName: "每周", usedPercent: 28 }]
        }, {
            providerName: "未配置",
            plans: []
        }, {
            providerName: "Codex",
            plans: [{ planName: "每周", usedPercent: 67 }]
        }]

        compare(MockData.nextProviderIndexWithUsage(providers, 0), 1)
        compare(MockData.nextProviderIndexWithUsage(providers, 1), 2)
        compare(MockData.nextProviderIndexWithUsage(providers, 2), 0)
    }

    function test_manual_windows_create_runtime_snapshot() {
        var definitions = MockData.normalizeDefinitions([{
            id: "custom",
            providerName: "Custom",
            vendor: "自定义",
            plans: [{
                id: "five-hours",
                planName: "5h",
                unit: "次",
                sourceType: "manual",
                manualUsed: 25,
                limit: 100
            }]
        }])
        var snapshots = MockData.createSeedSnapshots(definitions)
        var display = MockData.buildDisplayProviders(definitions, snapshots)

        compare(snapshots.length, 1)
        compare(snapshots[0].statusLabel, "可用")
        compare(snapshots[0].plans[0].used, 25)
        compare(snapshots[0].plans[0].total, 100)
        compare(display[0].plans[0].usedPercent, 25)
        compare(display[0].vendor, "自定义")
    }

    function test_normalize_definitions_keeps_new_fields_and_old_defaults() {
        var definitions = MockData.normalizeDefinitions([{
            id: "custom",
            providerName: "Custom",
            vendor: "Vendor",
            plans: [{
                id: "month", planName: "month", unit: "tokens",
                resetVariable: "${resetAt}"
            }]
        }])

        compare(definitions[0].vendor, "Vendor")
        compare(definitions[0].plans[0].sourceType, "http-js")
        compare(definitions[0].plans[0].limit, 0)
        compare(definitions[0].plans[0].manualUsed, 0)
        compare(definitions[0].plans[0].resetVariable, "${resetAt}")
    }

    function test_seed_minimax_waits_for_real_backend() {
        var snapshots = MockData.createSeedSnapshots(MockData.SEED_PROVIDER_DEFINITIONS)
        var minimax = null
        for (var i = 0; i < snapshots.length; ++i) {
            if (snapshots[i].providerId === "minimax")
                minimax = snapshots[i]
        }

        verify(minimax !== null)
        compare(minimax.statusLabel, "未配置")
        compare(minimax.errorText, "")
        compare(minimax.plans.length, 0)
    }

    function test_replace_snapshot_is_immutable() {
        var before = [{
            providerId: "minimax",
            statusLabel: "未配置",
            errorText: "",
            plans: []
        }, {
            providerId: "codex",
            statusLabel: "可用",
            errorText: "",
            plans: []
        }]
        var live = {
            providerId: "minimax",
            statusLabel: "可用",
            errorText: "",
            plans: [{ planId: "general-weekly", used: 28, total: 100 }]
        }

        var after = MockData.replaceSnapshot(before, live)

        compare(before[0].statusLabel, "未配置")
        verify(after !== before)
        verify(after[0] !== live)
        compare(after[0].statusLabel, "可用")
        compare(after[0].plans[0].used, 28)
        compare(after[1].providerId, "codex")
    }

    function test_replace_snapshot_accepts_cpp_array_like_plans() {
        var cppLikeSnapshot = {
            providerId: "codex",
            statusLabel: "可用",
            errorText: "",
            plans: {
                0: {
                    planId: "5-hours",
                    planName: "5 小时",
                    used: 42.5,
                    total: 100,
                    unit: "%",
                    resetText: "07-22 18:00",
                    extraText: "",
                    isValid: true,
                    invalidReason: ""
                },
                length: 1
            }
        }

        var after = MockData.replaceSnapshot([], cppLikeSnapshot)

        compare(after.length, 1)
        verify(Array.isArray(after[0].plans))
        compare(after[0].plans.length, 1)
        compare(after[0].plans[0].used, 42.5)
    }

    function test_invalid_definitions_fall_back_to_seed() {
        var seedCount = MockData.SEED_PROVIDER_DEFINITIONS.length

        compare(MockData.normalizeDefinitions(null).length, seedCount)
        compare(MockData.normalizeDefinitions({}).length, seedCount)
        compare(MockData.normalizeDefinitions("not json").length, seedCount)
        compare(MockData.normalizeDefinitions([{
            id: "broken",
            providerName: "Broken",
            plans: {}
        }]).length, seedCount)
    }

    function test_codex_seed_never_claims_fake_usage() {
        var out = MockData.buildDisplayProviders(
            MockData.SEED_PROVIDER_DEFINITIONS,
            MockData.SEED_RUNTIME_SNAPSHOTS)
        var codex = providerById(out, "codex")

        verify(codex !== null)
        compare(codex.statusLabel, "未登录")
        compare(codex.plans.length, 0)
    }

}
