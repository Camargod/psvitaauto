# Tasks: aa-control-channel

Spec: ../aa-control-channel/spec.md
Plan: ../aa-control-channel/plan.md
Updated: 2026-09-16

Order matters. Mark `[x]` only when the DoD is proven.

## T1: aa_session message layer

- [x] done

DoD:

- [x] `aa_session_send` emits 2B BE msgid + framed + per-frame encrypted bytes
- [x] `aa_session_poll`/`next_message` reassemble and decrypt into (channel, msgid, payload)
- [x] Plain and encrypted round-trips pass

## T2: aa_version codec

- [x] done

DoD:

- [x] `aa_version_encode_request/response` + `decode_response` per byte layout
- [x] Round-trip passes (major=1, minor=1, status=MATCH)

## T3: aa_control state machine

- [x] done

DoD:

- [x] Implements VERSION → TLS → AUTH → SERVICE_DISCOVERY → CHANNEL_OPEN → ACTIVE
- [x] Event-driven, non-blocking
- [x] Sends SERVICE_DISCOVERY_RESPONSE with video + input descriptors

## T4: Mock peer + integration test

- [x] done

DoD:

- [x] `tests/test_control.c` runs a scripted mock phone peer over loopback
- [x] Full handshake reaches ACTIVE
- [x] Ping request is answered
- [x] `ctest` passes all 7 tests

## Note (deviation)

- nanopb generation uses `-s max_size:128 -s max_count:8` (not just max_size);
  max_count avoids callback repeated fields, and 8 (not 32) keeps the
  generated `ServiceDiscoveryResponse` under nanopb's 64 KB field-info limit.
