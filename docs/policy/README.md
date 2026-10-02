# Im_player Governance Index

## Canonical policies

| ID | Document | Authority |
|---|---|---|
| IP-POL-001 | [Canonical Issue Execution & Integration Policy](./PROJECT_EXECUTION_POLICY.md) | Primary process authority for Issues, branches, PRs, audits, integration and completion |
| IP-COORD-001 | [Multi-Agent Coordination & Integration Wave Policy](./MULTI_AGENT_COORDINATION_POLICY.md) | Cross-issue/multi-agent coordination and integration planning authority subordinate to IP-POL-001 |
| IP-BUILD-001 | [Canonical Build, Dependency & Validation Policy](./BUILD_POLICY.md) | Build/dependency/test authority subordinate to IP-POL-001 |

## Mandatory GitHub surfaces

- `.github/ISSUE_TEMPLATE/canonical_work_item.yml`
- `.github/ISSUE_TEMPLATE/integration_wave.yml`
- `.github/ISSUE_TEMPLATE/config.yml`
- `.github/pull_request_template.md`
- `CONTRIBUTING.md`

## Rule

If a template, old checkpoint, audit, plan, comment or local convention conflicts with IP-POL-001, the canonical policy wins unless an explicit policy exception has been approved.
