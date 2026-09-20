# Tasks: aa-media-channels

Spec: ../aa-media-channels/spec.md
Plan: ../aa-media-channels/plan.md
Updated: 2026-09-16

Order matters. Mark `[x]` only when the DoD is proven.

## T1: Refactor aa_control (ACTIVE + multi-channel open + dispatch)

- [x] done

DoD:

- [x] Control channel becomes ACTIVE after SERVICE_DISCOVERY_RESPONSE
- [x] CHANNEL_OPEN_RESPONSE answered on any channel
- [x] Message dispatch branches by (channel, message id)

## T2: AV setup + video focus

- [x] done

DoD:

- [x] SETUP_REQUEST → SETUP_RESPONSE (status OK, max_unacked 1)
- [x] VIDEO_FOCUS_INDICATION (FOCUSED) after video setup + on focus request

## T3: Media dispatch (frames + ACK)

- [x] done

DoD:

- [x] Start/stop track per-channel session
- [x] Video H.264 dispatched to callback + AV_MEDIA_ACK per frame
- [x] Audio frames consumed and ACKed, not dispatched to the video callback

## T4: Input binding

- [x] done

DoD:

- [x] BINDING_REQUEST → BINDING_RESPONSE (OK)

## T5: Extended mock peer + integration test

- [x] done

DoD:

- [x] Mock peer opens video/input/audio channels, sets up, focuses, streams a
      video frame + audio frame, binds input, pings
- [x] `test_control` asserts all responses + the received H.264 frame
- [x] `ctest` passes all 7 tests
