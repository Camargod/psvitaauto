# Spec: aa-control-channel

Status: approved
Created: 2026-09-16
Last updated: 2026-09-16
Approved: 2026-09-16 (user)

## Overview

Implement the Android Auto control-channel message layer and the head-unit
client handshake state machine, wiring together the transport, TLS, framing,
and protobuf layers built so far. This is where the head unit actually
establishes an AA session with the phone.

## Goals

- `aa_session`: a message layer that sends/receives AA messages with a 2-byte
  big-endian message-ID prefix, per-frame TLS encryption, and correct
  channel/message-type/frame flags.
- A version request/response codec (raw big-endian, not protobuf).
- A client handshake state machine: VERSION → TLS → AUTH → SERVICE_DISCOVERY →
  CHANNEL_OPEN → ACTIVE.
- Host tests including a full handshake against a mock phone peer.

## Non-goals

- Media (video/audio) channel data flow — follow-up `aa-media-channels`.
- Input event generation — follow-up `aa-input`.
- Wireless bootstrap and on-device (Vita) usage.

## Handshake (normative, head-unit client role)

On the CONTROL channel (id 0), over TCP port 5277:

| # | Message (id) | Direction | Enc |
|---|---|---|---|
| 1 | VERSION_REQUEST (0x0001) | HU→phone | PLAIN |
| 2 | VERSION_RESPONSE (0x0002) | phone→HU | PLAIN |
| 3 | SSL_HANDSHAKE (0x0003) ×N | both | PLAIN |
| 4 | AUTH_COMPLETE (0x0004) | HU→phone | PLAIN |
| 5 | SERVICE_DISCOVERY_REQUEST (0x0005) | phone→HU | ENCRYPTED |
| 6 | SERVICE_DISCOVERY_RESPONSE (0x0006) | HU→phone | ENCRYPTED |
| 7 | CHANNEL_OPEN_REQUEST (0x0007) | phone→HU | ENCRYPTED, CONTROL |
| 8 | CHANNEL_OPEN_RESPONSE (0x0008) | HU→phone | ENCRYPTED, CONTROL |
| 9 | PING_REQUEST/RESPONSE (0x000b/0x000c) | HU→phone / phone→HU | PLAIN |

- VERSION_REQUEST body = 4 bytes BE: `uint16 major=1`, `uint16 minor=1`.
- VERSION_RESPONSE body = 6 bytes BE: `uint16 major`, `uint16 minor`,
  `uint16 status` (`MATCH=0`, `MISMATCH=0xFFFF`).
- SSL_HANDSHAKE loop: drive `aa_tls_handshake`; on each inbound SSL_HANDSHAKE,
  feed the record bytes in; send produced records out; repeat until the TLS
  handshake reports complete, then send AUTH_COMPLETE.

## Requirements

### Functional

- F1. `aa_session` sends a message `(channel_id, message_id, message_type,
  encryption, payload)` producing correct wire bytes: 2-byte BE message ID +
  framed, with per-frame TLS encryption when `encryption==ENCRYPTED`.
- F2. `aa_session` receives wire bytes and yields a reassembled, decrypted
  message `(channel_id, message_id, payload)`.
- F3. Version codec encodes/decodes per the byte layout above.
- F4. The client state machine runs the ordered handshake and reaches ACTIVE.
- F5. Ping/pong keepalive exchanges correctly.

### Non-functional

- N1. Host-compilable C11.
- N2. No phone dependency; tests use a mock peer built from the same primitives.

## Acceptance criteria

- AC1. Host build compiles the new modules and tests.
- AC2. `aa_session` plain round-trip preserves `(channel_id, message_id, payload)`.
- AC3. `aa_session` encrypted round-trip (via a TLS session) decrypts back to
  the original payload.
- AC4. Version codec round-trips (`major=1`, `minor=1`, `status=MATCH`).
- AC5. The client state machine completes a full handshake against a mock peer
  (loopback) and reaches ACTIVE.
- AC6. A ping request is answered by a ping response in the active session.
- AC7. `ctest` runs all unit tests and they pass.

## Constraints

- Message IDs and channel IDs come from `aa_msg.h` / `aa_constants.h`.
- The state machine must be event-driven (feed inbound bytes, emit outbound
  bytes via callbacks), not blocking, so it can later run on the Vita.

## Dependencies

- `aa-transport`, `aa-tls`, `aa-proto`.
- Research: `specs/research/aa-control-handshake.md`, `specs/research/aa-tls-mechanics.md`.

## Risks & unknowns

- Channel-id numeric values (VIDEO=3, INPUT=1) are aasdk's; a real-phone spike
  would confirm (see `aa-control-handshake.md` blockers).
- Mock peer validates the local logic only; interop with a real phone is
  deferred to the desktop harness (Phase 2).

## Open questions

- Ping cadence and whether the phone sends the first ping (defer: implement a
  responder + a simple 5s requester).
