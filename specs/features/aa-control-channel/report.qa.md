# QA Report: aa-control-channel

Date: 2026-09-16
Spec revision: approved, 2026-09-16

## Acceptance criteria

| # | Criterion | Result | Evidence |
|---|---|---|---|
| AC1 | Host build compiles the new modules and tests | PASS | `rm -rf build-host && cmake -S . -B build-host && cmake --build build-host` — configures and builds cleanly; `session.c`, `version.c`, `control.c` compile into `libcore.a` (CMakeLists.txt:43-57) and `test_session`, `test_control` link (tests/CMakeLists.txt:23-29). Build ends `[100%] Built target test_control`. |
| AC2 | `aa_session` plain round-trip preserves `(channel_id, message_id, payload)` | PASS | `./build-host/tests/test_session` → `RUN test_plain_roundtrip`, `25 checks, 0 failures`, `ALL PASSED`. Code path: `aa_session_send` (session.c:100-140) emits 2-byte BE msgid + framed bytes; `aa_session_poll`/`next_message` (session.c:142-233) reassemble and yield `(channel_id, message_id, payload)`. test_session.c:38-71 asserts channel/message-id/payload equality. |
| AC3 | `aa_session` encrypted round-trip decrypts back to original payload | PASS | `./build-host/tests/test_session` → `RUN test_encrypted_roundtrip` included in `25 checks, 0 failures`. test_session.c:102-141 performs a real mbedTLS handshake, sends `AA_ENC_ENCRYPTED`, then asserts decrypted payload equality. session.c:75-82 (encrypt) and 167-179 (decrypt). |
| AC4 | Version codec round-trips (`major=1`, `minor=1`, `status=MATCH`) | PASS | `./build-host/tests/test_session` → `RUN test_version_codec`, part of `25 checks, 0 failures`. test_session.c:22-36 encodes request (4B BE), encodes response (6B BE), decodes and asserts `maj==1`, `min==1`, `status==AA_VERSION_MATCH`. version.c:3-43 implements the exact byte layout. |
| AC5 | Client state machine completes a full handshake vs mock peer (loopback) and reaches ACTIVE | PASS | `./build-host/tests/test_control` → `5 checks, 0 failures`, `ALL PASSED`. test_control.c:121-159 drives HU `aa_control_poll` vs mock phone over `socketpair`; asserts `AA_CONTROL_ACTIVE` (test_control.c:150) and mock stage `6` (done). control.c:23-31,169-273 runs ST_SEND_VERSION → ST_WAIT_VERSION → ST_TLS → ST_WAIT_SERVICE_DISCOVERY → ST_WAIT_CHANNEL_OPEN → ST_ACTIVE. |
| AC6 | A ping request is answered by a ping response in the active session | PASS | `./build-host/tests/test_control` — mock sends `PING_REQUEST` (test_control.c:96-107), control.c:250-266 decodes `PingRequest` and replies `PING_RESPONSE` with matching timestamp; `TEST_ASSERT_TRUE(ph.got_ping_response)` (test_control.c:151) passes. |
| AC7 | `ctest` runs all unit tests and they pass | PASS | `ctest --test-dir build-host --output-on-failure` → `100% tests passed, 0 tests failed out of 7` (core, framing, transport, tls, proto, session, control). |

## Normative handshake cross-check

Message IDs and enc/type flags in `src/core/control.c` and `src/core/aa_msg.h` match the spec table:

| Spec row | Spec (id, dir, enc) | Code evidence |
|---|---|---|
| 1 VERSION_REQUEST (0x0001) | HU→phone PLAIN | control.c:47-48 `AA_MSG_VERSION_REQUEST` + `AA_ENC_PLAIN`; aa_msg.h:10 `0x0001` |
| 2 VERSION_RESPONSE (0x0002) | phone→HU PLAIN | mock test_control.c:47-48 PLAIN; aa_msg.h:11 `0x0002` |
| 3 SSL_HANDSHAKE (0x0003) | both PLAIN | control.c:56-57 `AA_ENC_PLAIN`; aa_msg.h:12 `0x0003` |
| 4 AUTH_COMPLETE (0x0004) | HU→phone PLAIN | control.c:75-76 `AA_ENC_PLAIN`; aa_msg.h:13 `0x0004` |
| 5 SERVICE_DISCOVERY_REQUEST (0x0005) | phone→HU ENCRYPTED | mock test_control.c:76-77 `AA_ENC_ENCRYPTED`; aa_msg.h:14 `0x0005` |
| 6 SERVICE_DISCOVERY_RESPONSE (0x0006) | HU→phone ENCRYPTED | control.c:110-111 `AA_ENC_ENCRYPTED`; aa_msg.h:15 `0x0006` |
| 7 CHANNEL_OPEN_REQUEST (0x0007) | phone→HU ENCRYPTED, CONTROL | mock test_control.c:91-92 `AA_MSG_CONTROL` + `AA_ENC_ENCRYPTED`; aa_msg.h:16 `0x0007` |
| 8 CHANNEL_OPEN_RESPONSE (0x0008) | HU→phone ENCRYPTED, CONTROL | control.c:125-126 `AA_MSG_CONTROL` + `AA_ENC_ENCRYPTED`; aa_msg.h:17 `0x0008` |
| 9 PING_REQ/RESP (0x000b/0x000c) | PLAIN | control.c:262-263 `AA_ENC_PLAIN`; mock test_control.c:105-106 PLAIN; aa_msg.h:18-19 `0x000b`/`0x000c` |

Version bodies: `aa_version_encode_request` emits 4B BE (version.c:3-16); `aa_version_encode_response` emits 6B BE (version.c:18-33); `AA_VERSION_MATCH=0x0000`, `AA_VERSION_MISMATCH=0xFFFF` (version.h:7-8). State machine is event-driven/non-blocking: `aa_control_poll` reads via `aa_transport_recv` with timeout 0 (session.c:145) and never blocks.

## Bugs found

None.

## Non-functional checks

- Host build: PASS — `cmake -S . -B build-host && cmake --build build-host` (0 errors, targets test_core…test_control built).
- Tests: PASS — `./build-host/tests/test_session` (25 checks, 0 failures), `./build-host/tests/test_control` (5 checks, 0 failures), `ctest --test-dir build-host --output-on-failure` (7/7 passed).
- Vita build regression: PASS — `VITASDK=/opt/homebrew/vitasdk make` produced `build/toolchain-setup.vpk` (71966 bytes, built target `toolchain-setup.vpk-vpk`).
- Lint/style: NOT CONFIGURED

## Verdict

PASS
