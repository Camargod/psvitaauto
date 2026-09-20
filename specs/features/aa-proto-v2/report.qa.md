# QA Report: aa-proto-v2

Date: 2026-09-16
Spec revision: approved, 2026-09-16

## Acceptance criteria

| # | Criterion | Result | Evidence |
|---|---|---|---|
| AC1 | Host build compiles with modern schemas | PASS | `cmake --build build-host` (0 errors); 253 generated `.pb.c` compiled into `libcore.a` |
| AC2 | Mock peer completes full handshake + media flow | PASS | `ctest` 8/8 (test_control: 19 checks, full handshake + video config) |
| AC3 | Real phone (AA 17.6 / Android 16) streams H.264 | PASS | `aa_harness` → VERSION 1.7 → TLS → ServiceDiscovery (6 channels) → audio focus → channel open (6) → media setup → video focus → **1395 H.264 frames, 8 MB** in 30 s; session stable (no disconnect) |

## Bugs found

None.

## Non-functional checks

- Host build: PASS
- Tests: PASS (8/8)
- Vita build regression: NOT RUN this pass
- Lint/style: NOT CONFIGURED

## Verdict

PASS
