## 1. Mock Data Source

- [ ] 1.1 Create `package/contents/js/mockData.js` with `MockProviders` array matching the current `providers` structure
- [ ] 1.2 Add `stripProviderSuffix(name)` function to mockData.js
- [ ] 1.3 Add `fluctuateProviders(providers)` function to randomly adjust usedPercent ±5%

## 2. Timer Integration

- [ ] 2.1 Add `import "js/mockData.js" as MockData` to main.qml
- [ ] 2.2 Create `Timer` component with `interval: 60000`, `running: true`, `repeat: true`
- [ ] 2.3 Add `onTriggered` handler to update `providers` with fluctuated data

## 3. Bug Fixes

- [ ] 3.1 Fix ProviderGroup.qml: remove duplicate `border.color` assignment (keep only the switch expression)
- [ ] 3.2 Fix ProviderGroup.qml: change error label visibility from `errorText.length > 0 && plans.length === 0` to `errorText.length > 0`
- [ ] 3.3 Fix main.qml: apply `stripProviderSuffix()` to provider names in tightestProviderName()
- [ ] 3.4 Fix Orb.qml: handle `tightestUsedPercent() < 0` with gray ring and "—" label

## 4. Cleanup

- [ ] 4.1 Remove unused import from configGeneral.qml
- [ ] 4.2 Run `qmllint` on all QML files and fix any errors
- [ ] 4.3 Update README.md with `plasmawindowed aiUsageWatcher` development instructions

## 5. Verification

- [ ] 5.1 Run `plasmawindowed aiUsageWatcher` and verify compact orb displays
- [ ] 5.2 Click orb to expand full view and verify all providers render
- [ ] 5.3 Wait 60s and verify timer triggers data refresh with fluctuated values
- [ ] 5.4 Verify color transitions: red (≤5%), yellow (≤15%), green (>15%)
- [ ] 5.5 Verify error state displays when provider has errorText and empty plans