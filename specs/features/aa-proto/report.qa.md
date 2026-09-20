# QA Report: aa-proto

Date: 2026-09-16
Spec revision: approved, 2026-09-16

## Acceptance criteria

| # | Criterion | Result | Evidence |
|---|---|---|---|
| AC1 | `tools/gen_proto.sh` reproduces the committed generated C | PASS | `cp -r src/core/aa_proto_generated /tmp/gen_qa && ./tools/gen_proto.sh && diff -rq /tmp/gen_qa src/core/aa_proto_generated` → script printed `generated 96 .pb.c files into …/src/core/aa_proto_generated`; `diff -rq` produced no output, exit code 0 (byte-identical). |
| AC2 | Host build compiles the generated C and `aa_msg` helpers | PASS | `cmake -S . -B build-host && cmake --build build-host` completed to `[100%] Built target test_proto` with no errors. `libcore.a` linked, including all 96 generated `*.pb.c` object files plus `aa_msg.c` (CMakeLists.txt:50,56-57). |
| AC3 | Round-trip encode/decode of `PingRequest` and `ChannelOpenRequest` recovers identical field values | PASS | `./build-host/tests/test_proto` → `RUN test_constants` / `RUN test_ping_roundtrip` / `RUN test_channel_open_roundtrip` / `12 checks, 0 failures` / `ALL PASSED`, exit 0. `test_proto.c:15-43` round-trips `timestamp`, `priority`, `channel_id`. |
| AC4 | `aa_msg` constants equal 3, 1, 7, 0xb | PASS | `src/core/aa_msg.h:10,12,16,18`: `AA_MSG_VERSION_REQUEST 0x0001`, `AA_MSG_SSL_HANDSHAKE 0x0003`, `AA_MSG_CHANNEL_OPEN_REQUEST 0x0007`, `AA_MSG_PING_REQUEST 0x000b`. Cross-check `third_party/aasdk_proto/ControlMessageIdsEnum.proto`: `VERSION_REQUEST = 0x0001`, `SSL_HANDSHAKE = 0x0003`, `CHANNEL_OPEN_REQUEST = 0x0007`, `PING_REQUEST = 0x000b`. Match confirmed. |
| AC5 | `ctest` runs all unit tests and they pass | PASS | `ctest --test-dir build-host --output-on-failure` → `100% tests passed, 0 out of 5 failed` (core_test, framing_test, transport_test, tls_test, proto_test), exit 0. |

## Bugs found

None. All five acceptance criteria pass.

### Note (non-blocking, spec-internal observation)

F4 lists `VersionRequest` among messages needing an encode/decode helper, but the spec's own Non-goals state raw-byte messages (`VERSION_REQUEST/RESPONSE`) are handled by `aa-control-channel`, not this feature. Consistent with that, there is no `VersionRequestMessage.proto` in the vendored schemas and thus no protobuf struct/helper for it; the `AA_MSG_VERSION_REQUEST` constant (F3/AC4) is defined and correct. No action required for the ACs.

Also noted (not a failure): the implementation exposes generic `aa_msg_encode`/`aa_msg_decode` helpers (descriptor-based) rather than the per-message named wrappers sketched in `plan.md`'s illustrative interface. This still satisfies F4's "helpers exist for these messages" (e.g. `test_proto.c` encodes/decodes `PingRequest` and `ChannelOpenRequest` through them), and `plan.md` explicitly marked the helper set as "finalized during implementation".

## Non-functional checks

- Host build: PASS — `cmake -S . -B build-host && cmake --build build-host` (100% built, no errors/warnings from `core`).
- Tests: PASS — `ctest --test-dir build-host --output-on-failure` → `100% tests passed, 0 out of 5 failed`.
- Reproducibility (AC1): PASS — regenerated output is byte-identical to committed output (`diff -rq` clean).
- Vita build regression: PASS — `VITASDK=/opt/homebrew/vitasdk make` produced `build/toolchain-setup.vpk` (71,966 bytes), exit 0.
- Lint/style: NOT CONFIGURED

## Verdict

PASS
