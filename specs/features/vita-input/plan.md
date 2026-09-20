# Plan: vita-input

Status: approved
Spec: ../vita-input/spec.md (approved)
Created: 2026-09-16

## Architecture

`aa_control_send_touch`/`send_key` build an `InputReport` (touch/key) and send
it on the input channel via the session. The Vita app captures SceTouch/SceCtrl
and calls these, scaling touch coords to the video frame space.

## File layout

```
src/core/control.h control.c   # + send_touch / send_key
src/vita/vita_video.c          # + input capture
tests/test_input.c             # round-trip
```

## AC mapping

| AC | Satisfied by |
|---|---|
| AC1 | test_input |
| AC2 | Vita make |

## Build & test strategy

- Host: `ctest` (input_test).
- Vita: `make` → `psvitaauto.vpk`.
