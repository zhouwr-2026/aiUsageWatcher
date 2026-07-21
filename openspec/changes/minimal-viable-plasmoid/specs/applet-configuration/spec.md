# Applet configuration

## Requirements

- `contents/config/config.qml` SHALL register General and Providers through ConfigModel/ConfigCategory.
- Pages SHALL use KCM.SimpleKCM cfg_ properties and standard Apply/Cancel; no external KCM ID or direct configuration writes are allowed.
- General SHALL expose compactStyle, refreshIntervalSec 10..3600, opacityPercent 20..100 and keepPanelOpen.
- Providers SHALL edit a working definitions JSON by stable ID with add/edit/delete, confirmation, validation and provider-level template preview.
- colorScheme, groupBy and true always-on-top SHALL not be implemented.

## Scenarios

- Cancel leaves persisted configuration unchanged; Apply persists valid values.
- Duplicate IDs/names, empty provider/plan names, zero plans or missing template placeholders disable Save.
- A provider can be added, edited and deleted, then survives plasmoid restart after Apply.
