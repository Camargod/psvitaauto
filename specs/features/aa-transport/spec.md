# Spec: aa-transport

Status: approved
Created: 2026-09-15
Last updated: 2026-09-15
Approved: 2026-09-15 (user)

## Overview

Implement the transport and framing layers of the Android Auto head unit
protocol in `src/core`, fully host-testable. This is the deterministic
foundation: a TCP client (the head unit connects to the phone on port 5277)
plus the AA message framing codec. TLS is deliberately out of scope (see
Non-goals) and becomes the follow-up feature `aa-tls`.

## Goals

- A transport abstraction: connect to `(host, port)`, send/receive raw bytes,
  with a read timeout. Host backend via BSD sockets now; a Vita backend is
  added later behind the same interface.
- A framing codec that encodes messages into frames and decodes a byte stream
  back into reassembled messages.
- Host unit tests for framing (round-trip, fragmentation) and TCP (loopback).

## Non-goals

- TLS (decoupled record layer) — follow-up feature `aa-tls`.
- Any protobuf/channel/message semantics (message IDs, ChannelOpen, etc.) —
  follow-up `aa-proto`.
- On-device (Vita) networking.

## Framing format (normative)

Frame header: 2 bytes.

- byte 0: `channelId` (uint8).
  Control=0, Input=1, Sensor=2, Video=3, MediaAudio=4, SpeechAudio=5,
  SystemAudio=6, AVInput=7, Bluetooth=8, NONE=255.
- byte 1: flags bitfield:
  - bit 0 (`0x01`): FrameType FIRST
  - bit 1 (`0x02`): FrameType LAST
  - bit 2 (`0x04`): MessageType CONTROL (otherwise SPECIFIC)
  - bit 3 (`0x08`): EncryptionType ENCRYPTED (otherwise PLAIN)
  - bits 4–7: reserved, always 0

Length field follows the header, big-endian:

- `FIRST` frame → EXTENDED (6 bytes): `uint16` frame payload length, then
  `uint32` total message length.
- `MIDDLE` / `LAST` / `BULK` frame → SHORT (2 bytes): `uint16` frame payload
  length.

FrameType values: `MIDDLE=0`, `FIRST=1`, `LAST=2`, `BULK=3` (FIRST|LAST).
MessageType: `SPECIFIC=0`, `CONTROL=4`. EncryptionType: `PLAIN=0`, `ENCRYPTED=8`.
Max frame payload = `0x4000` (16384 bytes).

Reassembly: on `FIRST`, the total message length is known from the EXTENDED
field; subsequent frames are appended until the accumulated length reaches the
total message length. Fragmentation: a message larger than `0x4000` is split
into a `FIRST` frame (EXTENDED) followed by `0x4000`-byte `MIDDLE` frames and a
final `LAST` frame; a message that fits is sent as a single `BULK` frame.

## Requirements

### Functional

- F1. Transport connects to `(host, port)` and can send/recv raw bytes; reads
  support a timeout.
- F2. Framing encodes a message `(channelId, messageType, encryptionType,
  payload, totalMessageSize)` into correct wire bytes per the normative format.
- F3. Framing decodes a byte stream into frames and reassembles fragmented
  messages into a single contiguous buffer.

### Non-functional

- N1. `src/core` remains host-compilable (no Vita headers).
- N2. C11 only; BSD sockets + standard headers only (no third-party deps).

## Acceptance criteria

- AC1. Host build compiles the transport and framing code.
- AC2. Round-trip: a single-frame `BULK` message encoded then decoded yields
  identical `(channelId, messageType, encryptionType, payload)`.
- AC3. Fragmentation: a message larger than `0x4000` encodes as one `FIRST`
  (EXTENDED) frame plus one or more `MIDDLE`/`LAST` (SHORT) frames; reassembly
  reconstructs the exact payload.
- AC4. Length encoding: `FIRST` uses the 6-byte EXTENDED field (frame size +
  total message size); `MIDDLE`/`LAST`/`BULK` use the 2-byte SHORT field.
- AC5. TCP loopback: the transport connects to a localhost server, sends a
  buffer, and receives it back within the timeout.
- AC6. `ctest` runs all unit tests and they pass.

## Constraints

- Port and channel-id constants fixed per the normative format above.
- No allocation limits beyond what C11 provides; framing must not read past a
  frame boundary.

## Dependencies

- `project-scaffold` (dual build + test harness).
- Research: `specs/research/aa-framing.md`, `specs/research/android-auto-protocol.md`.

## Risks & unknowns

- BULK-frame length encoding (SHORT) should be re-verified against aasdk
  `FrameSize` during implementation; if aasdk emits EXTENDED for BULK, update
  the normative format and AC4.
- The byte-exact flags layout is derived from aasdk; a traffic capture would
  confirm, but unit tests against the documented layout are the acceptance bar.

## Open questions

- None blocking; TLS design decisions are deferred to `aa-tls`.
