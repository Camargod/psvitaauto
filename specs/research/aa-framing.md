# Research: aa-framing

Date: 2026-09-15
Question: byte-exact transport/TLS/framing details of the AA head unit protocol

## Findings

All findings below are facts read directly from the `f1xpl/aasdk` (branch
`development`) and `f1xpl/openauto` (branch `master`) source trees, with one
fact derived by decoding the embedded certificate with `openssl x509`. No
claims here are hypotheses except where explicitly labelled.

### 1. Frame header — exact 2-byte layout

Every AA message is a sequence of **frames** on the wire. Each frame begins
with a **2-byte header** (from `FrameHeader::getData()`, `FrameHeader.cpp`):

```
byte 0 : ChannelId                 (uint8, full byte)
byte 1 : flags = encryption | messageType | frameType   (bitfield)
```

Byte 1 bit layout (the three fields are OR'd together on serialize and masked
apart on parse):

```
bit 7..4   reserved / unused (always 0)
bit 3      EncryptionType flag   (ENCRYPTED = 1 << 3 = 0x08)
bit 2      MessageType   flag    (CONTROL   = 1 << 2 = 0x04)
bit 1      FrameType LAST        (LAST      = 1 << 1 = 0x02)
bit 0      FrameType FIRST       (FIRST     = 1 << 0 = 0x01)
```

Parse code (`FrameHeader.cpp` constructor from a buffer):
```cpp
channelId_    = buffer.cdata[0];
frameType_    = buffer.cdata[1] & FrameType::BULK;          // mask 0x03
encryptionType_ = buffer.cdata[1] & EncryptionType::ENCRYPTED; // mask 0x08
messageType_  = buffer.cdata[1] & MessageType::CONTROL;     // mask 0x04
```

### FrameType enum (numeric values — `FrameType.hpp`)

```cpp
enum class FrameType {
    MIDDLE = 0,          // 0b00 — continuation frame (not first, not last)
    FIRST  = 1 << 0,     // 1     — first frame of a multi-frame message
    LAST   = 1 << 1,     // 2     — last frame of a multi-frame message
    BULK   = FIRST | LAST // 3   — single-frame (whole) message
};
```

### MessageType enum (`MessageType.hpp`)

```cpp
enum class MessageType { SPECIFIC = 0, CONTROL = 1 << 2 };  // 0 or 4
```

### EncryptionType enum (`EncryptionType.hpp`)

```cpp
enum class EncryptionType { PLAIN = 0, ENCRYPTED = 1 << 3 };  // 0 or 8
```

### ChannelId enum (`ChannelId.hpp`)

```cpp
enum class ChannelId {
    CONTROL = 0, INPUT = 1, SENSOR = 2, VIDEO = 3,
    MEDIA_AUDIO = 4, SPEECH_AUDIO = 5, SYSTEM_AUDIO = 6,
    AV_INPUT = 7, BLUETOOTH = 8, NONE = 255
};
```

### 2. Frame size encoding (`FrameSize.hpp` / `FrameSize.cpp`)

The frame length is encoded **big-endian** and comes in two variants. There is
**no dedicated flag bit** for short vs extended — the variant is inferred from
the `FrameType`: a **FIRST** frame uses EXTENDED, every other frame type
(MIDDLE / LAST / BULK) uses SHORT (`MessageInStream.cpp`,
`receiveFrameHeaderHandler`; `MessageOutStream.cpp`, `compoundFrame`).

- **SHORT** — 2 bytes: big-endian `uint16_t` = **this frame's payload size**.
- **EXTENDED** — 6 bytes:
  - bytes 0..2: big-endian `uint16_t` = this frame's payload size;
  - bytes 2..6: big-endian `uint32_t` = **total message size** (full plaintext
    message body = 2-byte MessageId + protobuf), carried only on the FIRST
    frame of a fragmented message.

`FrameSize::getSizeOf(EXTENDED) == 6`, `getSizeOf(SHORT) == 2`.
Serialization (`FrameSize::getData`) writes the frame size first (big-endian
u16) and, for EXTENDED, appends the total size (big-endian u32). Parsing
(`FrameSize` from buffer) reads the frame size from bytes [0..2) in both cases
and the total size from bytes [2..6) only when the buffer is ≥ 6 bytes.

### 3. Message reassembly / fragmentation

**Max frame payload size:** `cMaxFramePayloadSize = 0x4000` = **16384 bytes**
(`MessageOutStream.hpp`).

**Reassembly** (`MessageInStream.cpp`):

1. Read 2-byte header → `FrameHeader`.
2. On the first frame of a message, create a `Message(channelId, encryptionType,
   messageType)`. Every subsequent frame must carry the **same** channelId,
   otherwise reject with `MESSENGER_INTERTWINED_CHANNELS`.
3. Frame-size-field length = 6 if `frameType == FIRST`, else 2.
4. Read the size field → `FrameSize` (frame size + optional total size).
5. Read exactly `frameSize.getSize()` payload bytes.
6. If `ENCRYPTED`: `cryptor.decrypt(payload)` → append plaintext to message.
   Else: append raw payload.
7. If `frameType == BULK || LAST`: message complete → resolve. Else (FIRST /
   MIDDLE): loop back to step 1 for the next header.

Note: aasdk's own reassembly **does not use** the EXTENDED `totalSize` field —
it just reads `frameSize` bytes per frame until BULK/LAST. The field is still
part of the wire format and must be written/parsed.

**Fragmentation** (`MessageOutStream.cpp`):

- If `message.payload().size() >= cMaxFramePayloadSize` (0x4000), split the
  message into FIRST + (MIDDLE)* + LAST, each frame carrying ≤ 0x4000 payload
  bytes. Otherwise emit a single **BULK** frame.
- `frameType = offset==0 ? FIRST : (remaining-size > 0 ? MIDDLE : LAST)`.
- `compoundFrame(frameType, payloadBuffer)`:
  - 2-byte header + (6-byte if FIRST else 2-byte) size field + payload;
  - if ENCRYPTED: `cryptor.encrypt(payload)` → appends a TLS record
    (ciphertext) and records its length; else append raw bytes;
  - `setFrameSize`: frame size = **post-encryption payload length** (ciphertext
    length for ENCRYPTED frames), total size (FIRST only) = **full plaintext**
    `message.payload().size()`.

**Encryption flag semantics (important):** the `ENCRYPTED` flag means that
frame's payload is a TLS record produced by the SSL session (see §5), not a
raw protobuf. `PLAIN` frames carry raw protobuf bytes. Both kinds share the
same TCP stream. Encryption is applied **per message/frame**, not as a
full-stream TLS wrapper.

### 4. Cryptor — hardcoded TLS client certificate (`Cryptor.cpp`)

Both strings are `static const std::string` members of `messenger::Cryptor`
in `src/Messenger/Cryptor.cpp`:

- `cCertificate` — a **PEM** string (`-----BEGIN CERTIFICATE-----…-----END
  CERTIFICATE-----`), loaded via `PEM_read_bio_X509_AUX`.
- `cPrivateKey` — a **PEM** string (`-----BEGIN RSA PRIVATE KEY-----…` —
  PKCS#1), loaded via `PEM_read_bio_PrivateKey`.

Decoded certificate facts (via `openssl x509 -text`):

- **Version 1** X.509 certificate, serial `0x1b` (27).
- Signature algorithm: **sha256WithRSAEncryption**.
- **Issuer:** `C=US, ST=California, L=Mountain View, O=Google Automotive Link`
  (no CN).
- **Subject:** `C=JP, ST=Tokyo, L=Hachioji, O=JVC Kenwood, OU=01` (no CN).
- Validity: 2014-07-04 → 2045-04-29.
- Public key: **RSA 2048-bit**, exponent 65537.
- **Not self-signed** — issuer (`Google Automotive Link`) ≠ subject (`JVC
  Kenwood`). It is a leaf certificate signed by the "Google Automotive Link"
  issuer; no CA/intermediate chain is bundled, only this leaf + its private key.
  The subject organization is `JVC Kenwood`, and the issuer organization is
  `Google Automotive Link`.

The private key is not reproduced here; it is an RSA 2048-bit key matching the
certificate's public key, stored as a PEM string in the same file.

### 5. SSL/TLS (`SSLWrapper.cpp`, `Cryptor.cpp`)

- TLS **client** method: `TLSv1_2_client_method()` (OpenSSL < 1.1.0) /
  `TLS_client_method()` otherwise. i.e. **TLS 1.2**.
- **Role = client**: `SSL_set_connect_state(ssl)`.
- **No server verification**: `SSL_set_verify(ssl, SSL_VERIFY_NONE, nullptr)`.
- **Client authentication**: `SSL_CTX_use_certificate` + `SSL_CTX_use_PrivateKey`
  with the §4 cert/key (the head unit presents the "Google Automotive
  Link"-issued cert as its client cert).
- **Memory BIOs**: `BIO_new(BIO_s_mem())` for both read and write;
  `SSL_set_bio(ssl, readBIO, writeBIO)`, each with write-buffer size
  `20 * 1024`.

**Critical architectural nuance — TLS is NOT a full-stream socket wrapper.**
The TLS record layer is decoupled from TCP and run **inside** the AA message
layer:

1. **Handshake in-band**: after TCP connect, the head unit and phone first
   exchange a plaintext `VERSION_REQUEST`/`VERSION_RESPONSE` (Control channel).
   The TLS handshake is then driven through **`SSL_HANDSHAKE` control messages**
   (`ControlMessage::SSL_HANDSHAKE`, `EncryptionType::PLAIN`,
   `MessageType::SPECIFIC`). `cryptor.doHandshake()` → `SSL_do_handshake()`; the
   raw TLS handshake records produced are read via `readHandshakeBuffer()` and
   sent as `SSL_HANDSHAKE` payload; inbound `SSL_HANDSHAKE` payload is fed to
   `writeHandshakeBuffer()`. `SSL_ERROR_WANT_READ` → continue; `SSL_ERROR_NONE`
   → handshake complete.
2. **Per-message record encryption**: after handshake, messages marked
   `ENCRYPTED` are encrypted by `cryptor.encrypt()` (SSL_write then read the
   write BIO) producing TLS application-data records embedded as the frame
   payload, and decrypted by `cryptor.decrypt()` (write the read BIO then
   SSL_read). `PLAIN` messages skip the SSL engine entirely.

In openauto's Control channel usage: `VERSION_REQUEST`, `SSL_HANDSHAKE`,
`AUTH_COMPLETE`, `PING_REQUEST` are **PLAIN**; `SERVICE_DISCOVERY_RESPONSE`,
`AUDIO_FOCUS_*`, `SHUTDOWN_*`, `NAVIGATION_FOCUS_*` are **ENCRYPTED**
(`ControlServiceChannel.cpp`). The media/data channels likewise carry
ENCRYPTED payloads.

### 6. TCP transport (`TCPTransport.cpp`, `TCPEndpoint.cpp`, openauto `ConnectDialog.cpp`)

- **Connect model**: the head unit (aasdk/openauto) is the **TCP client**; it
  connects to the **phone's IP on port 5277**:
  `tcpWrapper_.asyncConnect(*socket, ipAddress, 5277, ...)` in
  `src/autoapp/UI/ConnectDialog.cpp`. This is the wireless "head unit server"
  mode where the **phone listens** on 5277.
- `TCPTransport`/`TCPEndpoint` are byte-stream shims (async read/write over
  `boost::asio::ip::tcp::socket`). No pre-TLS plaintext preamble or magic bytes
  on the TCP socket (unlike USB/AOA mode, which does send version strings).
- The raw bytes on the socket are exactly the AA **frames** described in §1–§3.
  TLS is established *through* the message layer (§5), so there is no separate
  TLS record stream wrapping the socket.

## Sources (URLs)

- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/FrameType.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/MessageType.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/EncryptionType.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/ChannelId.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/FrameSizeType.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/FrameHeader.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/FrameHeader.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/FrameSize.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/FrameSize.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/MessageInStream.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/MessageInStream.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/MessageOutStream.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/MessageOutStream.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/Cryptor.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/ICryptor.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/Message.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Messenger/MessageId.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/MessageId.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Transport/SSLWrapper.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/Transport/ITransport.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Transport/TCPTransport.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/TCP/TCPEndpoint.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/include/f1x/aasdk/TCP/TCPEndpoint.hpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Messenger/Messenger.cpp
- https://raw.githubusercontent.com/f1xpl/aasdk/development/src/Channel/Control/ControlServiceChannel.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/master/src/autoapp/UI/ConnectDialog.cpp
- https://raw.githubusercontent.com/f1xpl/openauto/master/src/autoapp/Service/AndroidAutoEntity.cpp
- Repo trees: https://api.github.com/repos/f1xpl/aasdk/git/trees/development?recursive=1 , https://api.github.com/repos/f1xpl/openauto/git/trees/master?recursive=1

## Implications for PSVitaAuto (C reimplementation)

- The wire framing is small and self-contained: 2-byte header + big-endian size
  + payload. This is trivially portable to C on the Vita (no boost/asio needed);
  only `uint8/uint16/uint32` big-endian conversion is required.
- A C reimplementation needs, minimally, to reproduce: frame header packing
  (§1), short/extended size encoding keyed on `FrameType::FIRST` (§2), the
  FIRST/MIDDLE/LAST/BULK reassembly state machine (§3), the 0x4000 fragment
  threshold, and the 2-byte big-endian `MessageId` prefix inside every message
  payload (Context; not a framing-layer concern but required to route messages).
- The Vita is the **head unit** and therefore the **TLS client**: it must
  connect to the phone on TCP 5277 and present the "Google Automotive Link"
  leaf cert (§4) as a client certificate, with server verification disabled.
- mbedTLS can replace OpenSSL here, but the mapping is **not** the default
  socket-stream model. The AA protocol uses TLS with a decoupled record layer:
  handshake records exchanged via `SSL_HANDSHAKE` messages, and per-message
  records embedded in ENCRYPTED frames. mbedTLS must be driven via custom
  `f_send`/`f_recv` callbacks (or equivalent) that capture the produced record
  bytes into a buffer (the analogue of OpenSSL's memory BIOs) rather than a
  socket.
- Audio is explicitly out of scope for the Vita transport: the Vita only
  forwards input and renders video; AA audio channels are negotiated but not
  output (per project charter). The framing note applies identically to the
  video/input channels.

## Remaining unknowns

- **mbedTLS record-layer decoupling feasibility** (biggest one): whether
  mbedTLS can cleanly do (a) handshake with raw record exchange over an
  in-band message channel and (b) single-buffer "encrypt this → return TLS
  record bytes" / "decrypt this record → plaintext", equivalent to OpenSSL
  memory-BIO + `SSL_write`/`SSL_read`. Needs a spike. (mbedTLS
  `mbedtls_ssl_set_bio` with custom callbacks is the likely path, but the
  per-record encrypt/decrypt without a live socket must be validated.)
- **Phone-side (server) certificate verification behavior**: aasdk sets
  `SSL_VERIFY_NONE` on the *client* (head unit) side only. Whether the phone's
  head-unit server actually verifies the presented "Google Automotive Link"
  client cert, or accepts any/most client certs, is not visible in this
  codebase. (Community head units all ship this same cert, which suggests it is
  trusted/expected, but the exact acceptance conditions are unverified here.)
- **Exact cipher suites / TLS extensions** the phone's server negotiates with
  this client — aasdk uses `TLS_client_method()`/`TLSv1_2_client_method()` with
  OpenSSL defaults; the exact negotiated suite list is not pinned in code.
- **Which specific messages/channels are ENCRYPTED vs PLAIN** across the full
  protocol (only the Control channel's usage was enumerated from
  `ControlServiceChannel.cpp`); the video/input/sensor channel encryption flags
  were not all enumerated.
- **Wireless session initiation** (out of this note's scope but blocking): what
  triggers the phone to run its head-unit server and accept a connection on
  5277 in the first place (WiFi-direct/Bluetooth handoff vs. a companion app /
  settings toggle on a stock phone). Already flagged in
  `specs/research/android-auto-protocol.md`.

## Recommended next steps

1. Spike mbedTLS "decoupled record layer" on host (x86) first: TLS 1.2 client,
   the §4 cert/key, `MBEDTLS_SSL_VERIFY_NONE`, custom bio callbacks, and verify
   (a) handshake records can be produced/consumed as buffers and (b) single
   buffers can be encrypted/decrypted into/from TLS application-data records.
   Gate the `aa-transport` feature on this spike's result.
2. Pin down the exact cipher-suite/extensions by running a live OpenSSL-based
   head unit (openauto) against a phone and capturing the negotiated suite, or
   by inspecting the AA server's `ServerHello`. Feed the minimal cipher list
   into the mbedTLS config.
3. Enumerate ENCRYPTED-vs-PLAIN per channel/message (grep all `Channel/*`
   `send*`/`receive*` for `EncryptionType::`) so the C framing layer can decide
   per message without guessing.
4. Determine the wireless session initiation mechanism (phone-side) before
   finalizing the connectivity architecture.
5. Cross-check this note against `specs/research/android-auto-protocol.md` and
   `specs/research/aa-open-source-headunits.md` and update `STATUS.md` /
   `BACKLOG.md` if the "aa-transport" feature scope changes as a result.
