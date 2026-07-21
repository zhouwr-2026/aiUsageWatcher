# Runtime usage model

## Requirements

- KConfig SHALL persist provider definitions only; Timer/manual refresh SHALL NOT write runtime usage to KConfig.
- `usedPercent` SHALL be derived from independent finite `used` and positive `total`; it SHALL be -1 when invalid.
- Classification SHALL be green below 85, yellow from 85 through 94, red from 95, gray for invalid/no data.
- Tightest usage SHALL be the maximum valid `usedPercent`.
- Mock refresh SHALL immutably update `used` and regenerate percent labels, used/total text and classes together.
- Seed MiniMax SHALL be 88/yellow; Codex 503/750 SHALL derive 67/green.
- Invalid JSON, null, object, missing plans and duplicate IDs SHALL not crash QML and SHALL use deterministic normalization/fallback.

## Scenarios

- Given 84/85/94/95, classification returns green/yellow/yellow/red.
- Given plans 22, 88, 67, tightest returns 88.
- Given a refresh, input objects remain unchanged and output text matches output used/total.
- Given no valid plan, tightest returns -1 and UI displays gray `—`.
