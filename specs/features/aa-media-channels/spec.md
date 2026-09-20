# Spec: aa-media-channels

Status: approved
Created: 2026-09-16
Last updated: 2026-09-16
Approved: 2026-09-16 (user)

## Overview

Implement the media and input channel lifecycle on top of the control channel:
per-channel open, AV setup, video focus, start/stop, media frame dispatch and
ACK, audio consume-and-ACK (discarded, since audio stays on the car's
Bluetooth), and input binding. Video frames are dispatched to a callback for
the future Vita decoder.

## Goals

- Answer CHANNEL_OPEN_REQUEST for video (3), media audio (4), speech audio (5),
  system audio (6), and input (1) channels.
- AV channel setup: answer SETUP_REQUEST with SETUP_RESPONSE (status OK).
- Video focus: send VIDEO_FOCUS_INDICATION (FOCUSED) after setup and in reply
  to VIDEO_FOCUS_REQUEST.
- Start/stop: track session IDs; dispatch media frames; ACK every media frame.
- Audio: consume and ACK audio frames without rendering (keep the session alive).
- Input: answer BINDING_REQUEST with BINDING_RESPONSE.

## Non-goals

- Actual video decode/render (later Vita feature).
- Generating input events (follow-up `aa-input`).
- On-device usage.

## Message flow (normative, after control channel ACTIVE)

| Message (id) | Channel | Direction | Notes |
|---|---|---|---|
| CHANNEL_OPEN_REQUEST (0x0007) | video/audio/input | phone→HU | ENCRYPTED, CONTROL |
| CHANNEL_OPEN_RESPONSE (0x0008) | same | HU→phone | ENCRYPTED, CONTROL, Status OK/FAIL |
| SETUP_REQUEST (0x8000) | video/audio | phone→HU | config_index |
| SETUP_RESPONSE (0x8003) | same | HU→phone | media_status=OK(2), max_unacked=1, configs=[0] |
| VIDEO_FOCUS_INDICATION (0x8008) | video | HU→phone | mode=FOCUSED(1) |
| START_INDICATION (0x8001) | video/audio | phone→HU | session, config |
| AV_MEDIA_WITH_TIMESTAMP_INDICATION (0x0000) | video/audio | phone→HU | raw [8B ts] + H.264 / PCM |
| AV_MEDIA_INDICATION (0x0001) | video/audio | phone→HU | raw H.264 / PCM |
| AV_MEDIA_ACK_INDICATION (0x8004) | video/audio | HU→phone | session, value=1 |
| STOP_INDICATION (0x8002) | video/audio | phone→HU | session |
| BINDING_REQUEST (0x8002) | input | phone→HU | scan_codes |
| BINDING_RESPONSE (0x8003) | input | HU→phone | Status OK/FAIL |

Enums: `AVChannelSetupStatus { NONE=0, FAIL=1, OK=2 }`,
`VideoFocusMode { NONE=0, FOCUSED=1, UNFOCUSED=2 }`, `Status { OK=0, FAIL=1 }`.

## Requirements

### Functional

- F1. CHANNEL_OPEN_RESPONSE is sent for each opened video/audio/input channel.
- F2. SETUP_REQUEST on a video/audio channel is answered with SETUP_RESPONSE
  (media_status OK, max_unacked 1).
- F3. After video setup, VIDEO_FOCUS_INDICATION (FOCUSED) is sent.
- F4. Video H.264 frames are decoded from AV_MEDIA_* messages and dispatched via
  a callback, and each is ACKed.
- F5. Audio frames are consumed and ACKed without rendering.
- F6. BINDING_REQUEST on the input channel is answered with BINDING_RESPONSE.

### Non-functional

- N1. Host-compilable C11.
- N2. Testable against an extended mock peer (no phone).

## Acceptance criteria

- AC1. Host build compiles.
- AC2. A mock peer opening video, media audio, and input channels each receives
  a CHANNEL_OPEN_RESPONSE.
- AC3. A video SETUP_REQUEST receives a SETUP_RESPONSE with status OK.
- AC4. A VIDEO_FOCUS_INDICATION (FOCUSED) is sent after video setup.
- AC5. After START_INDICATION, a test H.264 frame dispatched to the callback is
  received, and an AV_MEDIA_ACK_INDICATION is sent.
- AC6. An audio channel opens, sets up, receives a frame that is ACKed and not
  dispatched to the video callback.
- AC7. An input BINDING_REQUEST receives a BINDING_RESPONSE (OK).
- AC8. `ctest` runs all tests and they pass.

## Constraints

- Message IDs from `aa_msg.h`; channel IDs from `aa_constants.h`.
- Media frames are raw bytes (not protobuf); ACK uses `session` from
  START_INDICATION.

## Dependencies

- `aa-control-channel`, `aa-proto`, `aa-session`.
- Research: `specs/research/aa-media-channels.md`.

## Risks & unknowns

- Whether media frames arrive with the ENCRYPTED flag set (inferred PLAIN; the
  mock peer sends PLAIN).
- Exact channel-open order is phone-driven; the implementation must not assume
  an order.
- Audio descriptors advertised-but-discarded vs omitted (unverified).

## Open questions

- None blocking; the mock peer models the phone side with PLAIN media frames.
