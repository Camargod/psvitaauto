# Plan: aa-proto-v2

Status: approved
Spec: ../aa-proto-v2/spec.md (approved)
Created: 2026-09-16

## Architecture

Same layered stack (transport → framing → TLS → protobuf → control/media); only
the protobuf schemas and the message-layer code change.

## Components

1. **Schemas** — `third_party/aap_protobuf/` (253 protos from LIVI) + the
   `HeadUnitInfo.vehicle_type` field added.
2. **Generation** — `tools/gen_proto.sh` regenerates nanopb C into
   `src/core/aa_proto_generated/` (path-preserved output; `PB_FIELD_32BIT`).
3. **Message layer** — `aa_constants.h` (channel IDs), `aa_msg.h` (message IDs),
   `control.c` (handshake + service discovery + audio focus + sensor).
4. **Harness/mock** — `mock_phone.c` and `harness.c` updated to modern schemas.

## File layout

```
third_party/aap_protobuf/          # modern schemas
src/core/aa_proto_generated/       # nanopb C output (path-preserved)
src/core/aa_constants.h            # channel IDs (modern numbering)
src/core/aa_msg.h                  # message IDs
src/core/control.c                 # control + media + sensor handling
src/host/mock_phone.c              # mock peer (modern schemas)
```

## AC mapping

| AC | Satisfied by |
|---|---|
| AC1 | build + ctest (8 suites) |
| AC2 | test_control (mock full handshake) |
| AC3 | real-phone harness run |

## Build & test strategy

- Host: `cmake -S . -B build-host && cmake --build build-host`.
- Test: `ctest --test-dir build-host --output-on-failure`.
- Real phone: `aa_harness --host 127.0.0.1 --output /tmp/session.h264`
  (via `adb forward tcp:5277 tcp:5277`).
