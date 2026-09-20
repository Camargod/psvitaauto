# Plan: aa-tls

Status: approved
Spec: ../aa-tls/spec.md (approved)
Created: 2026-09-15

## Architecture

`aa_tls_session` wraps mbedTLS with a memory-buffer transport: the `f_send`
callback appends produced TLS records to an internal growable buffer and the
`f_recv` callback drains a caller-fed input buffer. No sockets. The session
supports both roles (`is_server`) so a loopback handshake can be tested with
the same wrapper.

## Components

1. **mbedTLS provisioning** — host build links brew `mbedtls` (libmbedtls,
   libmbedx509, libmbedcrypto); Vita build will use the vdpm `mbedtls` package
   later.
2. **`tls.c`** — `aa_tls_session`: init (cert/key, verify none, TLS 1.2),
   handshake driver (`WANT_READ`/complete), per-frame encrypt/decrypt, free.
3. **`aa_cert.c`** — the AA client cert + private key as PEM constants
   (vendored from aasdk `Cryptor.cpp`).
4. **Tests** — `tests/test_tls.c` with a self-signed test cert/key fixture:
   loopback handshake (client vs server) and encrypt/decrypt round-trips.

## Data flow

caller → `aa_tls_handshake(in)` → mbedTLS → `f_send` buffers outbound records →
`out`; inbound records fed back via `in`. After handshake, `aa_tls_encrypt` /
`aa_tls_decrypt` map to `mbedtls_ssl_write` / `mbedtls_ssl_read`.

## File layout

```
src/core/
  tls.h tls.c           # aa_tls_session
  aa_cert.h aa_cert.c   # embedded AA cert/key PEM
tests/
  test_tls.c            # loopback handshake + round-trip
  test_certs.h          # static self-signed test cert/key PEM
```

## Interfaces

```c
typedef struct aa_tls_session aa_tls_session;

/* is_server: 0 = client (AA role), 1 = server (harness/tests).
   cert_pem/key_pem NULL + client => embedded AA cert. */
int aa_tls_init(aa_tls_session *s, int is_server, const char *cert_pem, const char *key_pem);
void aa_tls_free(aa_tls_session *s);

/* 0 = handshake complete, 1 = WANT_READ (need more inbound bytes), -1 = error */
int aa_tls_handshake(aa_tls_session *s,
                     const uint8_t *in, size_t in_len,
                     uint8_t *out, size_t out_cap, size_t *out_len);

/* 0 on success (out_len set), -1 on error */
int aa_tls_encrypt(aa_tls_session *s, const uint8_t *plain, size_t plain_len,
                   uint8_t *out, size_t out_cap, size_t *out_len);

/* returns plaintext length (>=0), -1 error, -2 WANT_READ */
int aa_tls_decrypt(aa_tls_session *s, const uint8_t *cipher, size_t cipher_len,
                   uint8_t *out, size_t out_cap);
```

## AC mapping

| AC | Satisfied by |
|---|---|
| AC1 | tls.c + test build (T1/T2) |
| AC2 | test_tls.c loopback handshake |
| AC3 | test_tls.c encrypt/decrypt round-trips |
| AC4 | tls.c handshake driver return codes |
| AC5 | ctest wiring |

## Build & test strategy

- Host: mbedTLS **3.6.7 vendored** in-tree at `third_party/mbedtls` (with its
  `framework` submodule), built via `add_subdirectory`. Vita will use the vdpm
  `mbedtls` package later (same upstream, but a later feature wires it in).
- One targeted patch to the vendored mbedTLS (`library/x509.c`,
  `mbedtls_x509_get_time`): tolerate a numeric `+HHMM`/`-HHMM` timezone offset
  in UTCTime/GeneralizedTime. The aasdk AA client cert uses `-0700` offsets
  that stock mbedTLS rejects; the patch preserves the exact cert bytes (so the
  signature stays valid for the phone) while letting mbedTLS parse them.
- Build: `cmake -S . -B build-host && cmake --build build-host`.
- Test: `ctest --test-dir build-host --output-on-failure`.
- Vita build unaffected (core not yet wired into the Vita target).
