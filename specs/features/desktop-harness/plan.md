# Plan: desktop-harness

Status: approved
Spec: ../desktop-harness/spec.md (approved)
Created: 2026-09-16

## Architecture

A host-only executable `aa_harness` links the `core` library and drives the
existing `aa_control` state machine, writing decoded H.264 to a file. A
reusable `mock_phone` (extracted from `test_control.c`) provides the in-process
phone peer for `--mock` mode and for tests.

## Components

1. **`mock_phone`** — reusable mock peer (`src/host/mock_phone.{h,c}`), records
   the advertised video resolution + all response flags.
2. **`aa_harness`** — CLI (`--host --port --output --mock --resolution --fps`),
   video dump, session logging.
3. **`aa_control_set_video_config`** — resolution/fps setter applied to the
   advertised `VideoConfig`.
4. **Docs** — README section on enabling head unit server mode.

## File layout

```
src/host/
  test_certs.h       # moved from tests/
  mock_phone.h mock_phone.c
  harness.c
src/core/control.h control.c   # + set_video_config
tests/test_control.c           # now uses mock_phone.h
```

## Interfaces

```c
/* control.h */
typedef enum { AA_VIDEO_480P = 1, AA_VIDEO_720P = 2, AA_VIDEO_1080P = 3 } aa_video_resolution;
typedef enum { AA_FPS_30 = 1, AA_FPS_60 = 2 } aa_video_fps;
void aa_control_set_video_config(aa_control *c, aa_video_resolution res, aa_video_fps fps);

/* mock_phone.h */
mock_phone *mock_phone_create(int fd);
void mock_phone_step(mock_phone *p);
int mock_phone_done(const mock_phone *p);
void mock_phone_result_get(const mock_phone *p, mock_phone_result *r);
```

## AC mapping

| AC | Satisfied by |
|---|---|
| AC1 | CMake `aa_harness` target |
| AC2 | harness `--mock` writes `out.h264` |
| AC3 | `--help` output |
| AC4 | `aa_control_set_video_config` + mock captures resolution |
| AC5 | ctest harness smoke |
| AC6 | README |

## Build & test strategy

- Host: `cmake -S . -B build-host && cmake --build build-host` → `aa_harness`.
- Smoke: `aa_harness --mock --output /tmp/out.h264` (exit 0 on success).
- Test: `ctest --test-dir build-host --output-on-failure` (harness smoke added).
