# QA Report: aa-media-channels

Date: 2026-09-16
Spec revision: approved, 2026-09-16

## Acceptance criteria

| # | Criterion | Result | Evidence |
|---|---|---|---|
| AC1 | Host build compiles | PASS | `cmake -S . -B build-host && cmake --build build-host` completes with no errors; `libcore.a` links and `tests/test_control` is produced (`[100%] Built target test_control`). |
| AC2 | Mock peer opening video, media audio, and input channels each receives a CHANNEL_OPEN_RESPONSE | PASS | `tests/test_control.c:317-319` asserts `ph.video_open_ok`, `ph.input_open_ok`, `ph.audio_open_ok`. `src/core/control.c:127-140` (`send_channel_open_response`) + `:284-287` (dispatch on `AA_MSG_CHANNEL_OPEN_REQUEST`). Runtime: `15 checks, 0 failures`. |
| AC3 | A video SETUP_REQUEST receives a SETUP_RESPONSE with status OK | PASS | `src/core/control.c:142-158` (`send_setup_response`) sets `media_status = ..._Enum_OK` (OK=2 confirmed in `AVChannelSetupStatusEnum.pb.h:16`). Test asserts `ph.setup_ok` (`test_control.c:217-219`). Runtime: pass. |
| AC4 | A VIDEO_FOCUS_INDICATION (FOCUSED) is sent after video setup | PASS | `src/core/control.c:160-174` (`send_video_focus`) sets `focus_mode = ..._Enum_FOCUSED` (FOCUSED=1 confirmed in `VideoFocusModeEnum.pb.h:15`); `:319-322` chains focus after video `SETUP_REQUEST`. Test asserts `ph.focus_ok` (`test_control.c:220-222`). Runtime: pass. |
| AC5 | After START_INDICATION, a test H.264 frame dispatched to the callback is received, and an AV_MEDIA_ACK_INDICATION is sent | PASS | `src/core/control.c:176-217` (`handle_start` stores session; `handle_media_frame` strips 8-byte BE timestamp, calls `video_cb` for `AA_CHANNEL_VIDEO`, sends ACK with `session`). Test: `test_control.c:228-232` asserts `ph.video_ack_ok`; `:327-329` asserts `received_ts == 0x1234`, `received_h264_len == 7`, first bytes `0x00…0x65`. Runtime: pass. |
| AC6 | An audio channel opens, sets up, receives a frame that is ACKed and not dispatched to the video callback | PASS | Opens + ACK asserted: `ph.audio_open_ok` (`test_control.c:210-212`), `ph.audio_ack_ok` (`:234-238`). No-dispatch proven by `received_h264_len == 7` (`:328`) — the audio frame (4 bytes) never overwrites the video buffer because `handle_media_frame` only calls `video_cb` when `ch == AA_CHANNEL_VIDEO` (`control.c:201`). Audio setup is handled by the shared media dispatch: `SETUP_REQUEST` case applies to any non-control/non-input channel (`control.c:317-323`), and `send_setup_response` is channel-agnostic. See BUG-1 (test-coverage gap). |
| AC7 | An input BINDING_REQUEST receives a BINDING_RESPONSE (OK) | PASS | `src/core/control.c:219-232` (`send_binding_response`, status OK) + `:309-315` (input-channel dispatch). Test asserts `ph.binding_ok` (`test_control.c:240-244`). Runtime: pass. |
| AC8 | `ctest` runs all tests and they pass | PASS | `ctest --test-dir build-host --output-on-failure` → `100% tests passed out of 7` (control_test included). |

Cross-check of message IDs / enums: `src/core/aa_msg.h` matches the spec table exactly (CHANNEL_OPEN_REQUEST 0x0007, CHANNEL_OPEN_RESPONSE 0x0008, AV_MEDIA_WITH_TIMESTAMP_INDICATION 0x0000, AV_MEDIA_INDICATION 0x0001, SETUP_REQUEST 0x8000, START_INDICATION 0x8001, STOP_INDICATION 0x8002, SETUP_RESPONSE 0x8003, AV_MEDIA_ACK_INDICATION 0x8004, VIDEO_FOCUS_INDICATION 0x8008; input BINDING_REQUEST 0x8002 / BINDING_RESPONSE 0x8003). Enums verified: `AVChannelSetupStatus {NONE=0, FAIL=1, OK=2}`, `VideoFocusMode {NONE=0, FOCUSED=1, UNFOCUSED=2}`, `Status {OK=0, FAIL=1}`. Per-channel session tracking via `session_by_channel[9]` (`control.c:50, 256-258`), reset to `-1` on STOP (`:330-334`).

## Bugs found

### BUG-1: test_control does not exercise audio SETUP_REQUEST (nor START_INDICATION on the audio channel)

- Severity: minor (test-coverage gap only; no functional defect)
- Repro steps: Inspect `tests/test_control.c`. The mock phone opens the media-audio channel (step 6) and later sends a raw PCM frame (step 10) but never calls `send_setup(ph, AA_CHANNEL_MEDIA_AUDIO)` nor `send_start(ph, AA_CHANNEL_MEDIA_AUDIO, …)`. Run `./build-host/tests/test_control`; note no audio `SETUP_RESPONSE` assertion exists.
- Expected: AC6 states an audio channel "opens, sets up, receives a frame". The test should drive audio `SETUP_REQUEST` (and, per the protocol, `START_INDICATION` with a session) and assert a `SETUP_RESPONSE` on the audio channel before streaming the PCM frame.
- Actual: Only open, ACK, and no-dispatch are asserted. The "sets up" clause is verified only by code inspection, not by an automated assertion.
- Evidence: `tests/test_control.c:205-212` (open → jump to `send_setup(video)` only, never audio), `:266-271` (audio frame sent without prior setup/start). `src/core/control.c:317-323` shows audio setup is in fact handled (so functionality is correct).

## Non-functional checks

- Host build: PASS — `cmake -S . -B build-host && cmake --build build-host` (AppleClang, 0 errors).
- Tests: PASS — `./build-host/tests/test_control` → `15 checks, 0 failures`; `ctest --test-dir build-host --output-on-failure` → `100% tests passed out of 7`.
- Vita build regression: PASS — `VITASDK=/opt/homebrew/vitasdk make` → `[100%] Built target toolchain-setup.vpk-vpk`; `build/toolchain-setup.vpk` produced (71966 bytes).
- Lint/style: NOT CONFIGURED

## Verdict

PASS
