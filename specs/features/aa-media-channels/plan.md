# Plan: aa-media-channels

Status: approved
Spec: ../aa-media-channels/spec.md (approved)
Created: 2026-09-16

## Architecture

Extend `aa_control` from a control-channel-only handshake into a full session
handler. After the control channel becomes ACTIVE (following
SERVICE_DISCOVERY_RESPONSE), a message dispatch handles media/input messages by
(channel, message id): channel open responses, AV setup, video focus,
start/stop, media frame dispatch + ACK, audio consume-and-ACK, and input
binding. Video H.264 frames are handed to a caller-registered callback.

## Components

1. **Multi-channel open** — respond CHANNEL_OPEN_RESPONSE on any channel.
2. **AV setup + video focus** — SETUP_RESPONSE (status OK) + VIDEO_FOCUS_INDICATION.
3. **Media dispatch** — start/stop session tracking, per-frame ACK, video
   callback, audio discard.
4. **Input binding** — BINDING_RESPONSE (OK).
5. **Extended mock peer + test** — drives the full media flow over loopback.

## Data flow

phone → channel open/setup/focus/start/media/binding → `aa_control` dispatch →
responses + video callback.

## File layout

```
src/core/control.h control.c   # extended state machine + video callback
src/core/aa_msg.h              # + AV_MEDIA message IDs
tests/test_control.c           # extended mock peer
```

## Interfaces

```c
typedef void (*aa_video_frame_cb)(void *ctx, uint64_t timestamp,
                                  const uint8_t *h264, size_t len);
void aa_control_set_video_callback(aa_control *c, aa_video_frame_cb cb, void *ctx);
```

## AC mapping

| AC | Satisfied by |
|---|---|
| AC1 | control.c compiles |
| AC2 | multi-channel open (test_control) |
| AC3 | send_setup_response |
| AC4 | send_video_focus |
| AC5 | handle_media_frame + video callback + ACK |
| AC6 | audio consume-and-ACK (no video callback) |
| AC7 | send_binding_response |
| AC8 | ctest |

## Build & test strategy

- Build: `cmake -S . -B build-host && cmake --build build-host`.
- Test: `ctest --test-dir build-host --output-on-failure`; `test_control` runs
  the full media flow against the mock peer.
