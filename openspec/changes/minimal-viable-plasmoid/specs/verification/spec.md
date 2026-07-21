# Release verification

## Requirements

- qmltestrunner, qmllint and xmllint SHALL pass.
- metadata SHALL have ID aiUsageWatcher and nonempty Name/License.
- Installed copy SHALL match `package` and use the aiUsageWatcher directory.
- plasmawindowed logs SHALL contain no ReferenceError, TypeError, PlasmaCore.Units or component load errors.
- Visible verification SHALL confirm three providers and five PlanBars; timeout 124 alone SHALL not count as success.
