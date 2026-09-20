# Plan: aa-proto

Status: draft
Spec: ../aa-proto/spec.md (approved)
Created: 2026-09-16

## Architecture

nanopb generates one `.pb.c/.pb.h` pair per `.proto` into
`src/core/aa_proto_generated/` (committed in-tree). A thin `aa_msg` layer
(`aa_msg.h`/`aa_msg.c`) exposes the AA message-ID constants and encode/decode
helpers on top of the generated structs. The framing layer (`aa-transport`)
carries the resulting byte buffers; `aa-tls` encrypts the non-plain frames.

## Components

1. **Vendoring** — `third_party/nanopb` (0.4.9.2, generator + runtime pb_*.c/h)
   and `third_party/aasdk_proto` (the aasdk `.proto` schemas).
2. **Generation** — `tools/gen_proto.sh` runs `protoc` with the nanopb plugin
   over all schemas; `aa.options` sets a global string `max_size`.
3. **Generated code** — committed under `src/core/aa_proto_generated/`.
4. **`aa_msg`** — message-ID constants + `aa_msg_encode_*` / `aa_msg_decode_*`
   helpers for the control/input messages in the spec.
5. **Tests** — `tests/test_proto.c` round-trips.

## Data flow

app → `aa_msg_encode_*` → protobuf bytes → `aa-transport` frames → TLS →
peer; reverse via `aa_msg_decode_*`.

## File layout

```
third_party/nanopb/            # generator + runtime
third_party/aasdk_proto/       # *.proto + aa.options
tools/gen_proto.sh
src/core/aa_proto_generated/   # *.pb.c/*.pb.h (committed)
src/core/aa_msg.h  aa_msg.c
tests/test_proto.c
```

## Interfaces

```c
/* aa_msg.h */
#define AA_MSG_SSL_HANDSHAKE        0x0003
#define AA_MSG_VERSION_REQUEST      0x0001
#define AA_MSG_CHANNEL_OPEN_REQUEST 0x0007
#define AA_MSG_PING_REQUEST         0x000b
/* ... full set of spec message IDs ... */

int aa_msg_encode_ping_request(const PingRequest *m, uint8_t *out, size_t cap, size_t *len);
int aa_msg_decode_ping_request(const uint8_t *in, size_t len, PingRequest *m);
/* similar helpers for the control/input messages */
```

(Exact helper set finalized during implementation; the constants are the
normative part and must match the spec.)

## AC mapping

| AC | Satisfied by |
|---|---|
| AC1 | tools/gen_proto.sh reproducibility |
| AC2 | generated C + aa_msg compiled into core |
| AC3 | test_proto round-trips |
| AC4 | aa_msg.h constants |
| AC5 | ctest wiring |

## Build & test strategy

- Host: `brew install protobuf` (protoc) + `pip3 install protobuf grpcio-tools`
  for the nanopb generator (generation-time only, not build-time).
- Build: `cmake -S . -B build-host && cmake --build build-host`.
- Test: `ctest --test-dir build-host --output-on-failure`.
- Regeneration is a manual step; the build consumes committed generated code.
