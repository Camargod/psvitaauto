# QA Report: aa-transport

Date: 2026-09-15
Spec revision: approved, 2026-09-15

## Acceptance criteria

| # | Criterion | Result | Evidence |
|---|---|---|---|
| AC1 | Host build compiles the transport and framing code | PASS | `cmake -S . -B build-host && cmake --build build-host` — `src/core/frame.c`, `stream.c`, `transport.c` compile into `libcore.a` (all three `.o` built; "Built target core"). No Vita headers included. |
| AC2 | Round-trip: a single-frame `BULK` message encoded then decoded yields identical `(channelId, messageType, encryptionType, payload)` | PASS | `tests/test_framing.c:10-39` `test_bulk_roundtrip` encodes a BULK frame and asserts decode returns equal channel/message/encryption and `memcmp(payload)==0`. Direct run: `./build-host/tests/test_framing` → "132 checks, 0 failures". |
| AC3 | Fragmentation: a message larger than `0x4000` encodes as one `FIRST` (EXTENDED) frame plus one or more `MIDDLE`/`LAST` (SHORT) frames; reassembly reconstructs the exact payload | PASS | `tests/test_framing.c:84-118` `test_fragmentation_roundtrip` uses `msg_len = 0x4000*2+100` → `FIRST`+`MIDDLE`+`LAST` via `aa_stream_write_message`, then reassembles and asserts exact `memcmp` equality. `src/core/stream.c:102-149` confirms FIRST/EXTENDED then MIDDLE/LAST/SHORT emission. |
| AC4 | Length encoding: `FIRST` uses the 6-byte EXTENDED field (frame size + total message size); `MIDDLE`/`LAST`/`BULK` use the 2-byte SHORT field | PASS | `tests/test_framing.c:41-66` `test_length_encoding` asserts `FIRST` = 2+6+4=12 bytes, `LAST` = 2+2+4=8, `BULK` = 2+2+4=8. `src/core/frame.c:6` gates EXTENDED on `frame_type == FIRST` only, so `MIDDLE` shares the SHORT branch (verified end-to-end in the fragmentation round-trip, which would misparse otherwise). |
| AC5 | TCP loopback: the transport connects to a localhost server, sends a buffer, and receives it back within the timeout | PASS | `tests/test_transport.c:32-67` `test_loopback` binds an ephemeral `127.0.0.1` echo server, `aa_transport_connect`/`send`/`recv` (3000 ms timeout), asserts exact echo. Direct run: `./build-host/tests/test_transport` → "9 checks, 0 failures". |
| AC6 | `ctest` runs all unit tests and they pass | PASS | `ctest --test-dir build-host --output-on-failure` → "100% tests passed out of 3" (core_test, framing_test, transport_test). |

## Bugs found

None.

## Non-functional checks

- Host build: PASS — `export PATH="/opt/homebrew/bin:$PATH" && rm -rf build-host && cmake -S . -B build-host && cmake --build build-host` → all targets built, exit 0.
- Tests: PASS — `ctest --test-dir build-host --output-on-failure` → 3/3 passed; test binaries directly confirm assertions (`test_framing`: 132 checks, 0 failures; `test_transport`: 9 checks, 0 failures).
- Vita build regression: PASS — `export VITASDK=/opt/homebrew/vitasdk && export PATH="/opt/homebrew/bin:$VITASDK/bin:$PATH" && make` → produced `build/toolchain-setup.vpk` (71966 bytes).
- Lint/style: NOT CONFIGURED

## Verdict

PASS
