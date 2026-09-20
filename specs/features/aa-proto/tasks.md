# Tasks: aa-proto

Spec: ../aa-proto/spec.md
Plan: ../aa-proto/plan.md
Updated: 2026-09-16

Order matters. Mark `[x]` only when the DoD is proven.

## T1: Vendor nanopb + aasdk_proto + protoc

- [x] done

DoD:

- [x] `third_party/nanopb` at 0.4.9.2 (generator + runtime)
- [x] `third_party/aasdk_proto` holds the aasdk `.proto` schemas (96 files)
- [x] `protoc` (36.1) + nanopb generator deps installed and functional

## T2: Generation script + options + generated code

- [x] done

DoD:

- [x] `tools/gen_proto.sh` + global `max_size:128` via `-s` flag
- [x] `src/core/aa_proto_generated/` populated (96 `.pb.c` + `.pb.h`)
- [x] Re-running the script reproduces the committed output (diff clean)

## T3: aa_msg constants + helpers

- [x] done

DoD:

- [x] `aa_msg.h` defines the spec message-ID constants
- [x] `aa_msg.c` generic encode/decode helpers (nanopb)
- [x] Constants match the spec values (1, 3, 7, 0xb, 0x8001, ...)

## T4: Tests + ctest

- [x] done

DoD:

- [x] `tests/test_proto.c` round-trips PingRequest and ChannelOpenRequest
- [x] `ctest --test-dir build-host --output-on-failure` passes (5/5)
