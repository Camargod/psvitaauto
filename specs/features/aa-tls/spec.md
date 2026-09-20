# Spec: aa-tls

Status: approved
Created: 2026-09-15
Last updated: 2026-09-15
Approved: 2026-09-15 (user)

## Overview

Implement the Android Auto TLS layer as an mbedTLS session using the
"decoupled record layer" pattern aasdk uses: the TLS handshake is exchanged as
`SSL_HANDSHAKE` messages over the control channel, and post-handshake
application data is encrypted/decrypted per frame (the ENCRYPTED frame flag).
This is host-testable without a phone.

## Goals

- An mbedTLS session configured as a TLS 1.2 client with the AA client
  certificate and server verification disabled.
- A handshake driver that exchanges TLS records through memory buffers only
  (mbedTLS `f_send`/`f_recv` callbacks) — no live socket.
- Per-frame encrypt/decrypt primitives (`SSL_write`/`SSL_read` over buffers).
- Host self-tests: loopback handshake between two sessions and encrypt/decrypt
  round-trips.

## Non-goals

- Message-ID / channel integration: sending `SSL_HANDSHAKE` (messageId
  `0x0003`) and ENCRYPTED frames over the transport is `aa-control-channel`'s
  job. This feature only provides the TLS primitives + documents that contract.
- Real-phone interop (requires a device; deferred to a later validation step).

## Integration contract (normative reference)

- Handshake: TLS record bytes are carried as the payload of a Control-channel
  message with messageId `SSL_HANDSHAKE = 0x0003`, MessageType SPECIFIC,
  EncryptionType PLAIN.
- Application data: each outgoing frame chunk is `SSL_write`n into ciphertext
  that becomes the frame payload with the ENCRYPTED flag set; each incoming
  ENCRYPTED frame payload is fed to `SSL_read` to recover plaintext.

## Requirements

### Functional

- F1. Initialize a TLS 1.2 client session with a supplied or embedded
  certificate + private key; server certificate verification disabled.
- F2. The handshake driver advances the TLS handshake given inbound handshake
  ciphertext, producing outbound handshake ciphertext, and reports
  `WANT_READ` / `complete`.
- F3. Encrypt a plaintext buffer into a TLS record ciphertext buffer.
- F4. Decrypt a TLS record ciphertext buffer into a plaintext buffer.
- F5. The API accepts an externally supplied cert/key (for tests); the AA
  client cert/key are the embedded default.

### Non-functional

- N1. Host-compilable C11 (mbedTLS is C).
- N2. The TLS layer performs no network I/O (pure buffers + callbacks).

## Acceptance criteria

- AC1. Host build compiles the TLS layer and its tests.
- AC2. Loopback handshake: two mbedTLS sessions (client + server, self-signed
  certs) complete a TLS handshake exchanging only memory buffers.
- AC3. Encrypt/decrypt round-trip: plaintext encrypted by one side decrypts
  identically on the other, in both directions.
- AC4. The handshake driver returns `WANT_READ` when more inbound bytes are
  needed and signals completion when the handshake is done.
- AC5. `ctest` runs all unit tests and they pass.

## Constraints

- mbedTLS backend; TLS max version pinned to 1.2; client role;
  `MBEDTLS_SSL_VERIFY_NONE`.
- The AA client certificate and private key are vendored as PEM constants
  (extracted from aasdk `Cryptor.cpp`; GPL-3.0 origin — PSVitaAuto is
  effectively GPL-3.0 for this artifact).

## Dependencies

- `aa-transport` (defines the frame/message integration contract).
- Research: `specs/research/aa-tls-mechanics.md`, `specs/research/aa-framing.md`.

## Risks & unknowns

- Cipher-suite mismatch with a real phone (needs a real-phone spike later).
- aasdk assumes a whole TLS record arrives within one frame payload; unverified
  whether the phone splits/packs records across frames.
- Whether the phone sets ENCRYPTED on media (video/audio) frames is unverified.

## Open questions

- ~~mbedTLS provisioning for the host build?~~ → system package (brew) for
  host; vdpm `mbedtls` for Vita (same upstream).
