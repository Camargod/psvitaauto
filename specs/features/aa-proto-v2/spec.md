# Spec: aa-proto-v2

Status: approved
Created: 2026-09-16
Last updated: 2026-09-16
Approved: 2026-09-16 (user)

## Overview

Replace the stale aasdk protobuf schemas (pre-2023) with the modern Android
Auto `aap_protobuf` schemas and rewrite the message layer against them. The
original `aa-proto`/`aa-control-channel`/`aa-media-channels` were built on
aasdk protos; modern phones (AA 17.x) reject those messages.

## Goals

- Vendor the modern `aap_protobuf` schemas (from f-io/LIVI, GPL-3.0-or-later).
- Regenerate nanopb C bindings from them.
- Rewrite the control + media message layer against the modern schemas.
- Validate against a real phone (the `desktop-harness`).

## Non-goals

- Transport/framing/TLS changes (unchanged and already correct).
- On-device (Vita) integration.

## Key changes discovered during validation

- Protocol version: `major=1, minor=7` (aasdk's 1.1 is rejected).
- Channel IDs (modern numbering): sensor=1, video=2, input=3, speech audio=4,
  system audio=5, media audio=6 (aasdk used different values).
- `ServiceDiscoveryResponse` requires `make/model/year/vehicle_id/driver_position/...`
  and a `HeadUnitInfo` with `vehicle_type` (field 9, missing from the LIVI
  schema — added). `vehicle_type=3` (motorcycle) makes the phone record with
  its own microphone.
- `VideoConfiguration` requires `margin_width`/`margin_height` (fields 3/4).
- Audio: system audio (16 kHz mono) is the minimum; media+speech optional.
- Sensor channel: respond to `SENSOR_MESSAGE_REQUEST` and send
  `DrivingStatus` (parking brake unrestricted) to keep the session alive.

## Requirements

- F1. Modern `aap_protobuf` schemas vendored and nanopb C generated.
- F2. Control + media message layer rewritten against the modern schemas.
- F3. Sensor request/response + driving-status handling.
- F4. Full handshake + video streaming validated against a real phone.

## Acceptance criteria

- AC1. Host build compiles with the modern schemas (all tests green).
- AC2. A mock peer completes the full handshake + media flow.
- AC3. A real phone (AA 17.6, Android 16) completes the handshake and streams
  H.264 video continuously.

## Constraints

- Schemas from LIVI (GPL-3.0-or-later), compatible with the project's GPL-3.0.
- Message IDs and channel IDs match the modern protocol (per open-headunit).

## Dependencies

- `aa-transport`, `aa-tls` (unchanged).
- Research: `specs/research/aa-current-version.md`, `aa-error-2.md`,
  `aa-service-discovery.md`, `aa-head-unit-server.md`, `aa-channel-open-direction.md`.

## Open questions

- None blocking; the real-phone validation confirmed the schema.
