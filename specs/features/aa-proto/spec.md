# Spec: aa-proto

Status: approved
Created: 2026-09-16
Last updated: 2026-09-16
Approved: 2026-09-16 (user)

## Overview

Vendor the Android Auto protobuf schemas (aasdk) and generate C bindings with
nanopb, then expose message-ID constants and encode/decode helpers for the
head-unit control/input plane. This gives `aa-core` the typed message layer
that sits between the framing/TLS layers and the channel state machine.

## Goals

- Vendor nanopb (zlib) and the aasdk `.proto` schemas (GPL-3.0).
- Generate C code for the schemas with a reproducible script + `.options`
  (string `max_size`).
- Define the AA message-ID constants (per-channel enums, big-endian uint16).
- Provide encode/decode helpers for the head-unit control/input messages.
- Host unit tests: protobuf round-trips.

## Non-goals

- Raw-byte messages (VERSION_REQUEST/RESPONSE, SSL_HANDSHAKE, AV_MEDIA video
  frames) — their framing is handled by `aa-control-channel`/`aa-media-channels`.
- The channel state machine and sequencing.
- On-device (Vita) usage.

## Message IDs (normative, big-endian uint16)

- Control: `VERSION_REQUEST=0x0001`, `VERSION_RESPONSE=0x0002`,
  `SSL_HANDSHAKE=0x0003`, `AUTH_COMPLETE=0x0004`,
  `SERVICE_DISCOVERY_REQUEST=0x0005`, `SERVICE_DISCOVERY_RESPONSE=0x0006`,
  `CHANNEL_OPEN_REQUEST=0x0007`, `CHANNEL_OPEN_RESPONSE=0x0008`,
  `PING_REQUEST=0x000b`, `PING_RESPONSE=0x000c`.
- Input: `INPUT_EVENT_INDICATION=0x8001`, `BINDING_REQUEST=0x8002`,
  `BINDING_RESPONSE=0x8003`.
- AV/video: `SETUP_REQUEST=0x8000`, `START_INDICATION=0x8001`,
  `STOP_INDICATION=0x8002`, `SETUP_RESPONSE=0x8003`,
  `AV_MEDIA_ACK_INDICATION=0x8004`, `VIDEO_FOCUS_REQUEST=0x8007`,
  `VIDEO_FOCUS_INDICATION=0x8008`.

(These must match the aasdk `*MessageIdsEnum.proto` values; they are the source
of truth.)

## Requirements

### Functional

- F1. A reproducible script generates nanopb C from the vendored schemas.
- F2. The generated C compiles into the host `core` library.
- F3. Message-ID constants are defined and match the aasdk enum values.
- F4. Encode/decode helpers exist for at least: PingRequest/PingResponse,
  ChannelOpenRequest/Response, VersionRequest, ServiceDiscoveryRequest/Response,
  InputEventIndication.

### Non-functional

- N1. Generated code is committed in-tree (build must not require protoc).
- N2. String fields have explicit `max_size`; decode must not truncate
  realistic values (e.g. device name, head-unit name).

## Acceptance criteria

- AC1. `tools/gen_proto.sh` reproduces the committed generated C (verified by
  regenerating into a temp dir and diffing).
- AC2. Host build compiles the generated C and `aa_msg` helpers.
- AC3. Round-trip: encode then decode a `PingRequest` and a
  `ChannelOpenRequest` and recover identical field values.
- AC4. The `aa_msg` constants for `SSL_HANDSHAKE`, `VERSION_REQUEST`,
  `CHANNEL_OPEN_REQUEST`, and `PING_REQUEST` equal `3`, `1`, `7`, `0xb`.
- AC5. `ctest` runs all unit tests and they pass.

## Constraints

- nanopb 0.4.9.2 (zlib); aasdk schemas are GPL-3.0 (PSVitaAuto is effectively
  GPL-3.0 for these artifacts).
- All message IDs and field numbers come from the vendored schemas; do not
  hand-write them.

## Dependencies

- `aa-transport`, `aa-tls` (framing/stream consumers of these messages).
- Research: `specs/research/aa-proto-scope.md`, `specs/research/aa-tls-mechanics.md`.

## Risks & unknowns

- `max_size` values for string fields are estimates; a real-phone capture would
  confirm exact bounds.
- Whether the phone requires the full descriptor set (including sensor/nav) to
  start video — decides curated vs full closure (default: full closure).

## Open questions

- ~~Scope of generated closure?~~ → full closure (all 96 protos) for robustness.
