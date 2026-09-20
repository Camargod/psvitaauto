# Tasks: aa-transport

Spec: ../aa-transport/spec.md
Plan: ../aa-transport/plan.md
Updated: 2026-09-15

Order matters. Mark `[x]` only when the DoD is proven.

## T1: Frame codec + constants

- [x] done

DoD:

- [x] `aa_constants.h` defines channel IDs, frame/message/encryption types, `0x4000`
- [x] `aa_frame_encode` emits the 2-byte header + SHORT/EXTENDED length correctly
- [x] `aa_frame_decode` parses a frame and reports bytes consumed
- [x] BULK round-trip returns identical channel/message/encryption/payload

## T2: Message stream (fragment + reassemble)

- [x] done

DoD:

- [x] `aa_stream_write_message` emits BULK for small messages
- [x] Messages > `0x4000` emit FIRST (EXTENDED) + MIDDLE/LAST (SHORT)
- [x] `aa_stream_reader_feed` + `aa_stream_reader_next_message` reassemble the exact payload

## T3: Transport (BSD sockets)

- [x] done

DoD:

- [x] `aa_transport_connect/send/recv/close` implemented for host
- [x] `recv` respects the timeout
- [x] Loopback test connects, sends, and receives back within timeout

## T4: Tests + ctest wiring

- [x] done

DoD:

- [x] `tests/test_framing.c` covers AC2/AC3/AC4
- [x] `tests/test_transport.c` covers AC5
- [x] `ctest --test-dir build-host --output-on-failure` runs all and passes
