# Tasks: desktop-harness

Spec: ../desktop-harness/spec.md
Plan: ../desktop-harness/plan.md
Updated: 2026-09-16

Order matters. Mark `[x]` only when the DoD is proven.

## T1: aa_control video config setter

- [x] done

DoD:

- [x] `aa_control_set_video_config` sets resolution/fps
- [x] `send_service_discovery_response` uses the configured values

## T2: Extract mock_phone (reusable)

- [x] done

DoD:

- [x] `src/host/mock_phone.{h,c}` with create/step/done/result_get
- [x] Records the advertised video resolution
- [x] `tests/test_control.c` refactored to use it

## T3: aa_harness executable

- [x] done

DoD:

- [x] `src/host/harness.c` with CLI + video dump + `--mock`
- [x] `aa_harness` builds and runs `--mock`, writing `out.h264`

## T4: Docs + ctest

- [x] done

DoD:

- [x] README documents head unit server enablement + connect commands
- [x] ctest runs a harness smoke test and passes (8/8)
