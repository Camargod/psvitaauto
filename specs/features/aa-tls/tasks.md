# Tasks: aa-tls

Spec: ../aa-tls/spec.md
Plan: ../aa-tls/plan.md
Updated: 2026-09-16

Order matters. Mark `[x]` only when the DoD is proven.

## T1: mbedTLS provisioning + memory-BIO spike

- [x] done

DoD:

- [x] mbedTLS 3.6.7 vendored at `third_party/mbedtls`; host CMake builds libmbedtls/x509/crypto
- [x] Two-session memory-BIO loopback handshake succeeds (de-risks the pattern)

## T2: aa_tls session + handshake + encrypt/decrypt

- [x] done

DoD:

- [x] `aa_tls_create/destroy` configure client/server, verify none, TLS 1.2
- [x] `aa_tls_handshake` returns WANT_READ / complete correctly
- [x] `aa_tls_encrypt` / `aa_tls_decrypt` implemented via memory buffers

## T3: AA cert/key vendoring

- [x] done

DoD:

- [x] `aa_cert.h`/`aa_cert.c` hold the AA client cert + key PEM (from aasdk Cryptor.cpp)
- [x] The PEM loads successfully into mbedTLS (requires the x509 timezone-offset patch)

## T4: Self-tests + ctest

- [x] done

DoD:

- [x] `tests/test_tls.c` loopback handshake (client vs server) passes
- [x] Encrypt/decrypt round-trips pass in both directions
- [x] `ctest --test-dir build-host --output-on-failure` passes (4/4)
