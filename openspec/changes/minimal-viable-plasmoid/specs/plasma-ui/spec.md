# Plasma UI

## Requirements

- Compact SHALL support pie/bar and display the maximum usedPercent; compactStyle SHALL NOT affect full.
- Full SHALL render every provider through ProviderGroup and every plan through PlanBar; no inline alternative plan UI is allowed.
- Full SHALL expose refresh/config/keep-open ToolButtons with Breeze icons, tooltips and Accessible names; refresh SHALL rotate 300ms.
- Keep-open SHALL set `PlasmoidItem.hideOnWindowDeactivate`; it SHALL NOT claim WM always-on-top.
- UI SHALL use Kirigami Theme/Units, responsive Layouts, elide/wrap, a refresh/provider/plan status bar and a data-bearing compact tooltip.

## Scenarios

- Both compact styles produce the same three-provider/five-PlanBar full tree.
- Long names do not overlap the percent or action buttons.
- No data displays gray `—`; provider errors do not hide healthy providers.
