// SPDX-License-Identifier: GPL-2.0-or-later
//
// This file does NOT depend on any JS-side mock snapshot generator
// (providerSnapshot.js was removed in commit 2d497a9). tests/cpp/ tst_*
// files test the C++ client contracts; this file tests the pure-JS
// DisplayProvider / ProviderNormalize data-shape contract (sort
// orders, plan selection, threshold mapping, etc.).

import QtQuick 2.15
import QtTest 1.3
import "../package/contents/js/providerNormalize.js" as ProviderNormalize
import "../package/contents/js/displayProvider.js" as DisplayProvider
import "../package/contents/js/providerCatalog.js" as ProviderCatalog

TestCase {
    name: "TestDataContract"

    function providerById(providers, id) {
        for (var i = 0; i < providers.length; ++i) {
            if (providers[i].id === id)
                return providers[i]
        }
        return null
    }

    function providerIds(providers) {
        return providers.map(function(provider) { return provider.id }).join(",")
    }

    function sortableProviders() {
        return [{
            id: "unknown", providerName: "Zulu", tightestPercent: -1,
            tightestRemaining: -1, nextResetAt: -1
        }, {
            id: "high", providerName: "Beta", tightestPercent: 80,
            tightestRemaining: 20, nextResetAt: 2000
        }, {
            id: "low", providerName: "alpha", tightestPercent: 20,
            tightestRemaining: 80, nextResetAt: 1000
        }]
    }

    function test_sort_default_preserves_provider_order() {
        compare(providerIds(DisplayProvider.sortProviders(
                    sortableProviders(), "default", [])), "unknown,high,low")
    }

    function test_sort_alphabetical_uses_provider_name_a_to_z() {
        compare(providerIds(DisplayProvider.sortProviders(
                    sortableProviders(), "alphabetical", [])), "low,high,unknown")
    }

    function test_sort_used_percent_places_highest_first() {
        compare(providerIds(DisplayProvider.sortProviders(
                    sortableProviders(), "usedPercent", [])), "high,low,unknown")
    }

    function test_sort_remaining_percent_places_highest_first() {
        compare(providerIds(DisplayProvider.sortProviders(
                    sortableProviders(), "remainingPercent", [])), "low,high,unknown")
    }

    function test_sort_next_reset_places_earliest_known_time_first() {
        compare(providerIds(DisplayProvider.sortProviders(
                    sortableProviders(), "nextReset", [])), "low,high,unknown")
    }

    function test_sort_custom_uses_configured_provider_ids() {
        compare(providerIds(DisplayProvider.sortProviders(
                    sortableProviders(), "custom", ["high", "unknown", "low"])),
                "high,unknown,low")
    }

    function test_build_display_uses_earliest_plan_reset_time() {
        var definitions = [{
            id: "alpha", providerName: "Alpha", plans: [
                { id: "slow", planName: "Slow" },
                { id: "soon", planName: "Soon" }
            ]
        }]
        var snapshots = [{
            providerId: "alpha", plans: [
                { planId: "slow", used: 10, total: 100, resetAt: 2000 },
                { planId: "soon", used: 20, total: 100, resetAt: 1000 }
            ]
        }]

        compare(DisplayProvider.buildDisplay(definitions, snapshots)[0].nextResetAt, 1000)
    }

    function test_thresholds() {
        compare(ProviderNormalize.usageClass(84, "bar"), "bar-green")
        compare(ProviderNormalize.usageClass(85, "bar"), "bar-yellow")
        compare(ProviderNormalize.usageClass(94, "bar"), "bar-yellow")
        compare(ProviderNormalize.usageClass(95, "bar"), "bar-red")
        compare(ProviderNormalize.usageClass(-1, "bar"), "bar-gray")
    }

    function test_tightest_usage_uses_largest_valid_percent() {
        var tightest = DisplayProvider.tightestUsage([{
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

        var first = ProviderNormalize.providerUsageAt(providers, 0)
        var second = ProviderNormalize.providerUsageAt(providers, 1)
        var wrapped = ProviderNormalize.providerUsageAt(providers, 4)
        var empty = ProviderNormalize.providerUsageAt(providers, 2)

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

        compare(ProviderNormalize.nextProviderIndexWithUsage(providers, 0), 1)
        compare(ProviderNormalize.nextProviderIndexWithUsage(providers, 1), 2)
        compare(ProviderNormalize.nextProviderIndexWithUsage(providers, 2), 0)
    }

    function test_manual_windows_create_runtime_snapshot() {
        var definitions = ProviderNormalize.normalizeDefinitions([{
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
        var snapshots = ProviderNormalize.createSeedSnapshots(definitions)
        var display = DisplayProvider.buildDisplay(definitions, snapshots)

        compare(snapshots.length, 1)
        compare(snapshots[0].statusLabel, "可用")
        compare(snapshots[0].plans[0].used, 25)
        compare(snapshots[0].plans[0].total, 100)
        compare(display[0].plans[0].usedPercent, 25)
        compare(display[0].vendor, "自定义")
    }

    function test_normalize_definitions_keeps_new_fields_and_old_defaults() {
        var definitions = ProviderNormalize.normalizeDefinitions([{
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

    function test_native_seed_snapshots_are_empty() {
        var snapshots = ProviderNormalize.createSeedSnapshots(
            ProviderCatalog.defaultDefinitions())

        verify(Array.isArray(snapshots))
        compare(snapshots.length, 0)
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

        var after = ProviderNormalize.replaceSnapshot(before, live)

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

        var after = ProviderNormalize.replaceSnapshot([], cppLikeSnapshot)

        compare(after.length, 1)
        verify(Array.isArray(after[0].plans))
        compare(after[0].plans.length, 1)
        compare(after[0].plans[0].used, 42.5)
    }

    function test_invalid_definitions_fall_back_to_seed() {
        var seedCount = ProviderCatalog.defaultDefinitions().length

        compare(ProviderNormalize.normalizeDefinitions(null).length, seedCount)
        compare(ProviderNormalize.normalizeDefinitions({}).length, seedCount)
        compare(ProviderNormalize.normalizeDefinitions("not json").length, seedCount)
        compare(ProviderNormalize.normalizeDefinitions([{
            id: "broken",
            providerName: "Broken",
            plans: {}
        }]).length, seedCount)
    }

    function test_codex_no_backend_display_is_empty() {
        var out = DisplayProvider.buildDisplay(
            ProviderCatalog.defaultDefinitions(),
            [])
        var codex = providerById(out, "codex")

        verify(codex !== null)
        compare(codex.statusLabel, "暂无用量")
        compare(codex.plans.length, 0)
    }

    function test_usage_segments_are_limited_to_codexzh_weekly() {
        var definitions = [ProviderCatalog.definitionFor("codexzh"), {
            id: "other",
            providerName: "Other",
            plans: [{ id: "weekly", planName: "Weekly", unit: "USD" }]
        }]
        var segments = [
            { kind: "previous", used: 20, usedPercent: 20, formattedUsed: "$20.00" },
            { kind: "today", used: 30, usedPercent: 30, formattedUsed: "$30.00" }
        ]
        var display = DisplayProvider.buildDisplay(definitions, [{
            providerId: "codexzh", plans: [{
                planId: "weekly", used: 50, total: 100, usageSegments: segments
            }]
        }, {
            providerId: "other", plans: [{
                planId: "weekly", used: 50, total: 100, usageSegments: segments
            }]
        }])

        var codexZh = providerById(display, "codexzh")
        var otherProvider = providerById(display, "other")
        compare(codexZh.plans[0].usageSegments.length, 2)
        verify(!otherProvider.plans[0].hasOwnProperty("usageSegments"))

        display = DisplayProvider.buildDisplay(definitions, [{
            providerId: "codexzh", plans: [{
                planId: "weekly", used: 50, total: 100,
                usageSegments: [{ kind: "unknown", used: 20, usedPercent: 20 }]
            }]
        }])
        verify(!display[0].plans[0].hasOwnProperty("usageSegments"))
    }

    function test_usage_segments_accept_cpp_array_like_values() {
        var display = DisplayProvider.buildDisplay([ProviderCatalog.definitionFor("codexzh")], [{
            providerId: "codexzh", plans: [{
                planId: "weekly", used: 50, total: 100,
                usageSegments: {
                    0: { kind: "previous", used: 20, usedPercent: 20, formattedUsed: "$20.00" },
                    1: { kind: "today", used: 30, usedPercent: 30, formattedUsed: "$30.00" },
                    length: 2
                }
            }]
        }])

        compare(display[0].plans[0].usageSegments.length, 2)
        compare(display[0].plans[0].usageSegments[1].kind, "today")
    }

}
