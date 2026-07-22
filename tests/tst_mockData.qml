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
    }

    function test_provider_rotation_skips_providers_without_usage() {
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

        compare(MockData.nextProviderIndexWithUsage(providers, 0), 2)
        compare(MockData.nextProviderIndexWithUsage(providers, 2), 0)
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

    function test_minimax_not_fluctuated() {
        var before = [{
            providerId: "minimax",
            statusLabel: "可用",
            errorText: "",
            plans: [{ planId: "general-weekly", used: 28, total: 100 }]
        }]

        var after = MockData.fluctuateSnapshots(before, function() { return 1 })

        compare(after[0].plans[0].used, 28)
        verify(after !== before)
        verify(after[0] !== before[0])
        verify(after[0].plans[0] !== before[0].plans[0])
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

    function test_codex_derivation() {
        var out = MockData.buildDisplayProviders(
            MockData.SEED_PROVIDER_DEFINITIONS,
            MockData.SEED_RUNTIME_SNAPSHOTS)
        var codex = providerById(out, "codex")

        verify(codex !== null)
        compare(codex.plans[0].usedPercent, 67)
        compare(codex.plans[0].usedPercentLabel, "67%")
        compare(codex.plans[0].usedText, "503")
        compare(codex.plans[0].totalText, "750")
        compare(codex.plans[0].barClass, "bar-green")
        compare(codex.plans[0].templateText, codex.template)
    }

    function test_fluctuation_is_immutable_and_display_stays_in_sync() {
        var before = [{
            providerId: "sample",
            statusLabel: "可用",
            errorText: "",
            plans: [{
                planId: "quota",
                planName: "套餐",
                used: 40,
                total: 100,
                unit: "次",
                resetText: "明天",
                extraText: "",
                isValid: true,
                invalidReason: ""
            }]
        }]
        var definitions = [{
            id: "sample",
            providerName: "Sample",
            sourceLabel: "测试",
            template: "%1 %2/%3 %4",
            plans: [{ id: "quota", planName: "套餐", unit: "次" }]
        }]
        var after = MockData.fluctuateSnapshots(before, function() { return 1 })
        var display = MockData.buildDisplayProviders(definitions, after)

        compare(before[0].plans[0].used, 40)
        verify(after !== before)
        verify(after[0] !== before[0])
        verify(after[0].plans !== before[0].plans)
        verify(after[0].plans[0] !== before[0].plans[0])
        verify(after[0].plans[0].used !== before[0].plans[0].used)
        compare(after[0].plans[0].total, 100)
        compare(display[0].plans[0].usedText, String(after[0].plans[0].used))
        compare(display[0].plans[0].totalText, String(after[0].plans[0].total))
        compare(display[0].plans[0].usedPercent,
                Math.round(after[0].plans[0].used / after[0].plans[0].total * 100))
    }
}
