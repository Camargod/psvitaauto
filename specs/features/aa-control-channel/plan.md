# Plan: aa-control-channel

Status: draft
Spec: ../aa-control-channel/spec.md (approved)
Created: 2026-09-16

## Architecture

Three new `src/core` modules layered on the existing stack:

1. **`aa_session`** — the message layer. Owns a transport, a TLS session, and
   a frame-level read buffer. `send` builds `[msgid(2B BE)][payload]`, fragments
   into ≤0x4000 chunks, TLS-encrypts each chunk when ENCRYPTED, and frames it;
   `recv` decodes frames, decrypts per chunk, and reassembles into a message.
2. **`aa_version`** — raw big-endian version request/response codec.
3. **`aa_control`** — the head-unit client handshake state machine
   (IDLE → VERSION → TLS → AUTH → SERVICE_DISCOVERY → CHANNEL_OPEN → ACTIVE),
   event-driven and non-blocking.

## Components

- `src/core/session.h` / `session.c` — message send/recv with encryption.
- `src/core/version.h` / `version.c` — version codec.
- `src/core/control.h` / `control.c` — control state machine.
- `tests/test_session.c`, `tests/test_control.c` — unit + integration tests.

## Data flow

app → `aa_control` → `aa_session.send` → frames → transport → (mock phone) →
`aa_session.recv` → `aa_control` dispatch → reply.

## File layout

```
src/core/
  session.h session.c
  version.h version.c
  control.h control.c
tests/
  test_session.c
  test_control.c
```

## Interfaces

```c
/* session.h */
typedef struct aa_session aa_session;
aa_session *aa_session_create(aa_transport *t, aa_tls_session *tls);
void aa_session_destroy(aa_session *s);
int aa_session_send(aa_session *s, uint8_t channel_id, uint16_t message_id,
                    uint8_t message_type, uint8_t encryption,
                    const uint8_t *payload, size_t payload_len);
/* returns 0 when a full message is available, 1 when more data is needed, -1 error */
int aa_session_poll(aa_session *s);
int aa_session_next_message(aa_session *s, uint8_t *channel_id, uint16_t *message_id,
                            uint8_t **payload, size_t *payload_len);

/* version.h */
int aa_version_encode_request(uint16_t major, uint16_t minor, uint8_t *out, size_t cap, size_t *len);
int aa_version_encode_response(uint16_t major, uint16_t minor, uint16_t status, uint8_t *out, size_t cap, size_t *len);
int aa_version_decode_response(const uint8_t *in, size_t len, uint16_t *major, uint16_t *minor, uint16_t *status);

/* control.h */
typedef struct aa_control aa_control;
typedef enum { AA_CONTROL_IDLE, AA_CONTROL_ACTIVE, AA_CONTROL_FAILED } aa_control_state;
aa_control *aa_control_create(aa_transport *t);
void aa_control_destroy(aa_control *c);
/* drive: reads transport, advances handshake; returns current state */
aa_control_state aa_control_poll(aa_control *c);
aa_control_state aa_control_state(const aa_control *c);
```

(Exact signatures finalized during implementation; semantics fixed by the spec.)

## AC mapping

| AC | Satisfied by |
|---|---|
| AC1 | all modules compile |
| AC2 | session.c plain round-trip (test_session) |
| AC3 | session.c encrypted round-trip (test_session) |
| AC4 | version.c codec (test_session) |
| AC5 | control.c + test_control.c full handshake vs mock peer |
| AC6 | control.c ping + test_control.c |
| AC7 | ctest wiring |

## Build & test strategy

- Build: `cmake -S . -B build-host && cmake --build build-host`.
- Test: `ctest --test-dir build-host --output-on-failure`.
- The mock phone peer is implemented in `tests/test_control.c` using the same
  `aa_session`/`aa_tls`/`aa_msg` primitives over a loopback transport.
