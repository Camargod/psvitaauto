# Plan: aa-transport

Status: approved
Spec: ../aa-transport/spec.md (approved)
Created: 2026-09-15

## Architecture

Three layers in `src/core`, all C11 and host-compilable:

1. **Transport** (`transport.c`) — a BSD-socket backend behind a small
   `aa_transport` struct. Blocking connect/send; `recv` with a timeout.
2. **Frame codec** (`frame.c`) — encodes/decodes a single frame per the
   normative 2-byte header + short/extended length format.
3. **Message stream** (`stream.c`) — fragments an opaque message buffer into
   frames (`BULK`, or `FIRST`+`MIDDLE`+`LAST`), and reassembles a byte stream
   back into a full message buffer.

Shared constants live in `aa_constants.h`.

## Components

- `src/core/aa_constants.h` — channel IDs, frame/message/encryption types, max
  payload `0x4000`.
- `src/core/frame.h` / `frame.c` — `aa_frame` struct + `aa_frame_encode` /
  `aa_frame_decode`.
- `src/core/stream.h` / `stream.c` — `aa_stream_reader` (reassembly) and
  `aa_stream_write_message` (fragmentation).
- `src/core/transport.h` / `transport.c` — `aa_transport_connect/send/recv/close`.
- `tests/test_framing.c`, `tests/test_transport.c` — host unit tests.

## Data flow

app → `aa_stream_write_message` → frames → `aa_transport_send` → socket →
peer → socket → `aa_transport_recv` → `aa_stream_reader_feed` → message.

## File layout

```
src/core/
  aa_constants.h
  frame.h   frame.c
  stream.h  stream.c
  transport.h transport.c
tests/
  test_framing.c
  test_transport.c
```

## Interfaces

```c
/* frame.h */
typedef struct {
    uint8_t  channel_id;
    uint8_t  frame_type;      /* MIDDLE/FIRST/LAST/BULK */
    uint8_t  message_type;    /* SPECIFIC/CONTROL */
    uint8_t  encryption_type; /* PLAIN/ENCRYPTED */
    const uint8_t *payload;
    uint16_t payload_size;
    uint32_t total_message_size; /* meaningful only for FIRST */
} aa_frame;

int aa_frame_encode(const aa_frame *f, uint8_t *out, size_t out_cap, size_t *out_len);
int aa_frame_decode(const uint8_t *buf, size_t len, aa_frame *out, size_t *consumed);

/* stream.h */
int aa_stream_write_message(aa_stream_writer *w, uint8_t channel_id,
                            uint8_t message_type, const uint8_t *msg, size_t msg_len);
int aa_stream_reader_feed(aa_stream_reader *r, const uint8_t *data, size_t len);
int aa_stream_reader_next_message(aa_stream_reader *r, uint8_t *out, size_t out_cap, size_t *out_len);

/* transport.h */
int aa_transport_connect(aa_transport *t, const char *host, uint16_t port);
int aa_transport_send(aa_transport *t, const uint8_t *data, size_t len);
int aa_transport_recv(aa_transport *t, uint8_t *buf, size_t len, int timeout_ms);
void aa_transport_close(aa_transport *t);
```

(Exact signatures may be adjusted during implementation; semantics are fixed
by the spec.)

## AC mapping

| AC | Satisfied by |
|---|---|
| AC1 | frame.c + stream.c + transport.c compile in host build |
| AC2 | frame.c encode/decode round-trip (test_framing) |
| AC3 | stream.c fragmentation + reassembly (test_framing) |
| AC4 | frame.c length encoding (test_framing) |
| AC5 | transport.c loopback (test_transport) |
| AC6 | ctest wiring (tests/) |

## Build & test strategy

- Build: `cmake -S . -B build-host && cmake --build build-host`.
- Test: `ctest --test-dir build-host --output-on-failure`.
- The Vita build (`make`) is unaffected; a smoke `make` run is performed to
  confirm no regression.
