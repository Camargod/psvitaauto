# Feature Status Board

State machine: `spec` -> `plan` -> `implement` -> `qa` -> `done`

| Feature | Spec | Plan | QA | State | Notes |
|---|---|---|---|---|---|
| toolchain-setup | approved | approved | PASS | done | T1-T5 |
| project-scaffold | approved | approved | PASS | done | T1-T4 |
| aa-transport | approved | approved | PASS | done | T1-T4 |
| aa-tls | approved | approved | PASS | done | T1-T4 |
| aa-proto | approved | approved | PASS | done | T1-T4 |
| aa-control-channel | approved | approved | PASS | done | T1-T4 (superseded by aa-proto-v2) |
| aa-media-channels | approved | approved | PASS | done | T1-T5 (superseded by aa-proto-v2) |
| desktop-harness | approved | approved | PASS | done | T1-T4 |
| aa-proto-v2 | approved | approved | PASS | done | T1-T5 (modern schemas; real-phone validated) |
| wireless-bootstrap | approved | approved | PASS | done | T1-T3 (discovery + docs) |
| vita-video | approved | approved | PASS | done | T1-T4 (builds; on-device AC2-AC4 pending) |
| vita-input | approved | approved | PASS | done | T1-T3 (encoding + capture; on-device pending) |
| vita-ui | approved | approved | PASS | done | T1 (config + status; on-device pending) |
| packaging-release | approved | approved | PASS | done | T1 (LiveArea assets) |

## Legend

- State: `spec` (drafting / awaiting approval), `plan`, `implement`, `qa`,
  `done`.
- Update this file at every stage transition, in the same change as the
  artifact that caused it.
