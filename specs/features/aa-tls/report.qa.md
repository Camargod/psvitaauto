# QA Report: aa-tls

Date: 2026-09-16
Spec revision: approved, 2026-09-15

## Acceptance criteria

| # | Criterion | Result | Evidence |
|---|---|---|---|
| AC1 | Host build compiles the TLS layer and its tests | PASS | `cmake -S . -B build-host && cmake --build build-host` succeeded; `src/core/tls.c`, `src/core/aa_cert.c` compiled into `libcore.a`, and `tests/test_tls.c` linked into `build-host/tests/test_tls`. Build output: `Built target core`, `Built target test_tls`. |
| AC2 | Loopback handshake: two mbedTLS sessions (client + server, self-signed certs) complete a TLS handshake exchanging only memory buffers | PASS | `tests/test_tls.c:14-57` creates `aa_tls_create(1, TEST_CERT_PEM, TEST_KEY_PEM)` (server) and `aa_tls_create(0, NULL, NULL)` (client, embedded AA cert) and drives `aa_tls_handshake` by feeding each side's output buffer as the other's input — no socket. `TEST_ASSERT_TRUE(c_done)` and `TEST_ASSERT_TRUE(s_done)` both pass (handshake completes). Transport is `f_send`/`f_recv` callbacks over growable memory buffers (`src/core/tls.c:34-63`). |
| AC3 | Encrypt/decrypt round-trip: plaintext encrypted by one side decrypts identically on the other, in both directions | PASS | `tests/test_tls.c:59-74`: client `aa_tls_encrypt("hello from client")` → server `aa_tls_decrypt` yields identical plaintext (`memcmp` passes); server `aa_tls_encrypt("hello from server")` → client `aa_tls_decrypt` yields identical plaintext. Both `TEST_ASSERT_EQUAL_INT` length checks pass. |
| AC4 | Handshake driver returns `WANT_READ` when more inbound bytes are needed and signals completion when done | PASS | `src/core/tls.c:192-199` maps `mbedtls_ssl_handshake` result: `0` → `result = 0` (complete), `MBEDTLS_ERR_SSL_WANT_READ` → `result = 1` (WANT_READ), else `-1` (error). `tests/test_tls.c:24-54` exercises both: initial call returns non-negative (WANT_READ) and the loop terminates when `r == 0` (complete). Header documents the contract in `src/core/tls.h:17`. |
| AC5 | `ctest` runs all unit tests and they pass | PASS | `ctest --test-dir build-host --output-on-failure` → `100% tests passed out of 4` (core_test, framing_test, transport_test, tls_test). Direct run `./build-host/tests/test_tls` → `12 checks, 0 failures`, `ALL PASSED`, exit 0. |

## Integration contract verification (normative reference)

- TLS config pins TLS 1.2 client with server verification disabled: `src/core/tls.c:138` `mbedtls_ssl_conf_authmode(&s->conf, MBEDTLS_SSL_VERIFY_NONE)`; `src/core/tls.c:139-140` min/max version `MBEDTLS_SSL_VERSION_TLS1_2`; client role set via `MBEDTLS_SSL_IS_CLIENT` at `src/core/tls.c:129` when `is_server == 0`. Confirmed.
- Channel/type enums present in `src/core/aa_constants.h`: `aa_channel_id_t` (control=0 … none=255), `aa_message_type_t` (`AA_MSG_SPECIFIC = 0x00`), `aa_encryption_type_t` (`AA_ENC_PLAIN = 0x00`, `AA_ENC_ENCRYPTED = 0x08`). These match the spec's documented contract (SPECIFIC / PLAIN / ENCRYPTED). The `SSL_HANDSHAKE = 0x0003` messageId is out of scope for this feature per spec Non-goals ("aa-control-channel's job"), so its absence here is not a defect.
- Embedded AA client cert/key vendored as PEM constants in `src/core/aa_cert.c:8-61` (`aa_cert_pem` / `aa_key_pem`); loaded successfully into mbedTLS via `test_cert_loads` (`tests/test_tls.c:8-12`, `aa_tls_create(0, NULL, NULL)` returns non-NULL).
- Vendored mbedTLS timezone-offset patch present: `third_party/mbedtls/library/x509.c:679-692` tolerates a numeric `+HHMM`/`-HHMM` offset in `mbedtls_x509_get_time`. (Context only, not an AC.)

## Bugs found

None.

## Non-functional checks

- Host build: PASS — `export PATH="/opt/homebrew/bin:$PATH" && rm -rf build-host && cmake -S . -B build-host && cmake --build build-host` completed with no errors; all targets (mbedtls, mbedx509, mbedcrypto, core, test_core, test_framing, test_transport, test_tls) built.
- Tests: PASS — `ctest --test-dir build-host --output-on-failure` → `100% tests passed out of 4`; `./build-host/tests/test_tls` → `12 checks, 0 failures`, `ALL PASSED`, exit 0.
- Vita build regression: PASS — `export VITASDK=/opt/homebrew/vitasdk && export PATH="/opt/homebrew/bin:$VITASDK/bin:$PATH" && make` produced `build/toolchain-setup.vpk` (71966 bytes), exit 0.
- Lint/style: NOT CONFIGURED

## Verdict

PASS
