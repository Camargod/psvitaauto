# Research: aa-current-version

Date: 2026-09-16
Question: what version values does a current AA head unit send in VERSION_REQUEST?

## Findings

### TL;DR

A current Android Auto head unit sends **VERSION_REQUEST with major = 1, minor = 7**
(raw payload bytes `00 01 00 07`, big-endian). The message format is **unchanged**:
still a raw 4-byte payload (two big-endian uint16), NOT protobuf, carried on the
control channel (id 0) with message id `0x0001` and plaintext flags `0x03`. Our
harness currently sends major=1, minor=1 (aasdk's old default), which a 2026
phone (AA 16.x/17.x) rejects before TLS. The fix is a one-line version bump.

### Version values observed in active head-unit implementations (primary sources)

| Project (language) | major | minor | raw bytes | Last activity | Works w/ modern phone? |
|---|---|---|---|---|---|
| `f1xpl/aasdk` (C++, the library our harness derives from) | 1 | 1 | `00 01 00 01` | legacy | **No — rejected by AA 16/17** |
| `uglyoldbob/android-auto` (Rust crate) | 1 | 1 | `00 01 00 01` | 2026-07 | unchanged, same stale value |
| `andreknieriem/open-headunit` (Kotlin) | 1 | 2 | `00 01 00 02` | 2026-08 | Yes (reports AA 17.5 working) |
| `f-io/LIVI` `livi-aa` (Rust) | 1 | 7 | `00 01 00 07` | 2026-09 | Yes |
| `mrmees/openauto-prodigy` / `open-android-auto` (C++/Qt) | 1 | 7 | `00 01 00 07` | 2026-09 | Yes ("preferred") |

Citations:

- `aasdk/include/f1x/aasdk/Version.hpp`: `static const uint16_t AASDK_MAJOR = 1; AASDK_MINOR = 1;`
- `aasdk/src/Channel/Control/ControlServiceChannel.cpp` `sendVersionRequest()`:
  writes `native_to_big(AASDK_MAJOR)` at offset 0 and `native_to_big(AASDK_MINOR)`
  at offset 2 into a 4-byte buffer, payload after the 2-byte message id `VERSION_REQUEST`.
- `open-headunit/.../aap/protocol/messages/Messages.kt`: `VERSION_REQUEST = byteArrayOf(0, 1, 0, 2)`,
  sent via `createRawMessage(0, 3, 1, VERSION_REQUEST, VERSION_REQUEST.size)` → chan 0,
  flags 3, type 1 (VERSION_REQUEST), then 4 raw bytes.
- `LIVI/native/livi-helperd/crates/livi-aa/src/consts.rs`: `VERSION_MAJOR = 1`, `VERSION_MINOR = 7`.
- `LIVI/.../livi-aa/src/session.rs` `send_version_request()`:
  `VERSION_MAJOR.to_be_bytes()` + `VERSION_MINOR.to_be_bytes()`, sent with
  `frame::encode(CH_CONTROL, FLAGS_PLAINTEXT, CTRL_VERSION_REQUEST, &data)`.
  Its round-trip test (`frame.rs`) asserts the exact wire bytes for 1.7:
  header `[00, 03, 00, 06]` then type `00 01` then payload `00 01 00 07`.
- `openauto-prodigy/libs/prodigy-oaa-protocol/include/oaa/Version.hpp`:
  `PROTOCOL_MAJOR = 1`, `PROTOCOL_MINOR = 7` (unit-tested in `test_version.cpp`).
- `openauto-prodigy/.../src/Channel/ControlChannel.cpp` `sendVersionRequest(major, minor)`:
  `qToBigEndian(major)` at `[0..1]`, `qToBigEndian(minor)` at `[2..3]`, emitted as
  message `0x0001` on channel 0.

### VERSION_REQUEST message format is unchanged (raw 4 bytes, not protobuf)

All five implementations agree on the wire layout. The full frame (after the
6-byte header `[channel:1][flags:1][length:2][type:2]`) is:

```
channel   = 0x00  (CONTROL)
flags     = 0x03  (PLAINTEXT = FIRST|LAST)
length    = 0x0006  (big-endian; counts the 2-byte type + 4-byte payload)
type      = 0x0001  (VERSION_REQUEST)
payload   = 0x0001 0x0007  (major=1 BE, minor=7 BE)
```

The Prodigy protocol cross-reference (decompiled AA APK v16.1, Feb 2026) confirms
it against the phone side: `VERSION_REQUEST: 2 shorts (major, minor)` — raw bytes,
Car→Phone. It is explicitly NOT a protobuf message (unlike ServiceDiscovery,
ChannelOpen, etc.).

### What the phone replies (VERSION_RESPONSE)

The phone answers with a raw VERSION_RESPONSE (`type 0x0002`): `major(2B BE) +
minor(2B BE) + status(2B BE)`. `status == 0x0000` means match; `status == 0xffff`
means mismatch. `open-android-auto`'s `ControlChannel.cpp` parses exactly this
(`if dataSize < 6 → malformed`), and LIVI's `session.rs` aborts only when
`status == VERSION_STATUS_MISMATCH (0xffff)`. `aasdk` parses the same 3-short
layout. (Note: the Prodigy prose mentions "4 shorts (type, major, minor, status)"
for the response; the actual head-unit code reads 3 shorts and preserves any
trailing bytes — see "Remaining unknowns".)

### The version the phone accepts (minimum / preferred)

From the Prodigy cross-reference (Sony XAV-AX100 firmware + AA APK v16.1
decompiled, dated 2026-02-23):

> "Phone supports: v1.6 (minimum) → v1.7 (preferred), fallback cap v6.0"

and the "GAL version" policy admits exactly `1.7, 4.3, 5.0, 5.1, 6.0`; `1.7` is
the "legacy" value. Newer tuples (4.3+) unlock H.265, overlays, MediaOptions
(`0x8014`), etc.; `1.7` keeps the classic minimal obligations (H.264 video +
per-packet ACK flow control). So for a minimal head unit (render video + forward
input, no H.265/cluster/overlay), **1.7 is the correct, safe value**.

Fact vs hypothesis:

- **Fact (source-backed):** the two most sophisticated active head units (LIVI,
  open-android-auto/Prodigy) send `1.7`; open-headunit sends `1.2`; aasdk and the
  uglyoldbob crate still send `1.1`. The Prodigy research labels 1.7 "preferred"
  and 1.6 "minimum".
- **Hypothesis (consistent with all data, not directly observed):** the 2026 phone
  closes our connection because 1.1 is below the current admission floor. The floor
  is somewhere in `[1.2 .. 1.6]` — open-headunit's 1.2 is reported working on
  AA 17.5, while our 1.1 is rejected. Prodigy's "1.6 minimum" may be slightly
  conservative or may reflect the phone's own advertised version rather than the
  exact HU admission floor; either way 1.7 is above every plausible floor.

### Does the phone's "head unit server" (developer mode / DHU) still speak the same HUP?

Yes — the head-unit server is the phone's own AA car service listening for a local
head unit over adb/`5277`; it is the same GAL protocol (TLS 1.2 + protobuf), not a
fork. Evidence:

- `open-headunit` issue #874 ("OHU 3.2.6 and Android Auto 17.5", closed 2026-08-31):
  "starting AA headunit server works" on AA 17.5.
- Issue #956 ("Android Auto 17 developer mode warning dialog", closed): confirms
  the developer-mode head-unit server path still exists in AA 17.x.
- Issue #698 ("App is not working after android auto 17.4 release update") +
  #732 ("Android Auto 17.4 Self mode work around"): AA 17.4 *did* break the old
  head units and required adaptation, but the fix was in connection/developer-mode
  handling, not a protocol rewrite — open-headunit still speaks the same raw
  framing + TLS + protobuf on top of it.

### Other initial-handshake observations (unchanged, for context)

After VERSION_REQUEST/VERSION_RESPONSE, the sequence is unchanged: SSL_HANDSHAKE
(`0x0003`, TLS 1.2 with the Google Automotive Link CA), AUTH_COMPLETE (`0x0004`),
then ServiceDiscovery. `open-headunit` sends a raw "status OK" (`00 03 00 04 08 00`,
i.e. `AuthComplete`/status message) after TLS. No new mandatory handshake step was
observed in any 2024–2026 implementation; the only admission-relevant change is the
version floor.

## Sources (URLs)

- https://github.com/f1xpl/aasdk/blob/master/include/f1x/aasdk/Version.hpp
- https://github.com/f1xpl/aasdk/blob/master/src/Channel/Control/ControlServiceChannel.cpp
- https://github.com/andreknieriem/open-headunit/blob/main/app/src/main/java/com/andrerinas/openheadunit/aap/protocol/messages/Messages.kt
- https://github.com/andreknieriem/open-headunit/blob/main/app/src/main/java/com/andrerinas/openheadunit/aap/AapTransport.kt
- https://github.com/uglyoldbob/android-auto/blob/master/src/lib.rs (`const VERSION: (u16,u16) = (1,1);`)
- https://github.com/uglyoldbob/android-auto/blob/master/src/control.rs (version request encode)
- https://github.com/f-io/LIVI/blob/main/native/livi-helperd/crates/livi-aa/src/consts.rs
- https://github.com/f-io/LIVI/blob/main/native/livi-helperd/crates/livi-aa/src/session.rs
- https://github.com/f-io/LIVI/blob/main/native/livi-helperd/crates/livi-aa/src/frame.rs
- https://github.com/mrmees/openauto-prodigy/blob/main/docs/aa-protocol/android-auto-protocol-cross-reference.md
- https://github.com/mrmees/openauto-prodigy/blob/main/libs/prodigy-oaa-protocol/include/oaa/Version.hpp
- https://github.com/mrmees/openauto-prodigy/blob/main/libs/prodigy-oaa-protocol/src/Channel/ControlChannel.cpp
- https://github.com/mrmees/openauto-prodigy/blob/main/src/core/aa/GalVersionPolicy.cpp
- open-headunit issues: #698, #732, #874, #921, #956 (github.com/andreknieriem/open-headunit/issues/<n>)

## Implications for PSVitaAuto

- **Bump our VERSION_REQUEST from `(1,1)` to `(1,7)`.** This is a one-line change:
  emit payload `00 01 00 07` instead of `00 01 00 01`. Keep the raw 4-byte payload
  and the existing `[chan=0][flags=0x03][len][type=0x0001]` header exactly as-is.
- **Do not switch to protobuf** for the version request; the format is still raw.
- **1.7 (not 6.0)** is the right target: it keeps the classic minimal obligations
  (H.264 video, per-packet ACK flow control) that the Vita's decode/render path
  already assumes. Requesting 4.3+ would advertise capabilities we don't implement
  (H.265, overlay, MediaOptions), which is unnecessary and risky.
- When parsing VERSION_RESPONSE, read `major`, `minor`, `status` from the first 6
  bytes and treat `status == 0xffff` as a hard failure (log + abort). Tolerate a
  response longer than 6 bytes (trailing bytes are ignored), so we're robust to any
  "type"-prefixed variant the Prodigy prose hints at.
- Keep TLS 1.2 and the existing post-version handshake (SSL_HANDSHAKE → AUTH →
  ServiceDiscovery) unchanged; no other admission step changed.

## Remaining unknowns

- The exact minimum minor the phone enforces is not pinned by a single authoritative
  source: our 1.1 is rejected, open-headunit's 1.2 is reported working, and Prodigy
  says "1.6 minimum". The precise floor (1.2? 1.6?) is a hypothesis; 1.7 sidesteps it.
- The VERSION_RESPONSE payload length: head-unit code reads 3 shorts (6 bytes) and
  preserves any trailing bytes, while the Prodigy prose says "4 shorts (type, major,
  minor, status)". Whether newer phones prepend a "type" short is unverified from a
  live capture. Doesn't affect us (we only need major/minor/status).
- Whether the phone's admission check is a hard close (observed: closes TCP on 1.1)
  vs. a MISMATCH response that the HU must honor. Worth a capture after the bump.
- The AA 17.4 breakage (issue #698) was fixed in open-headunit without a version
  bump, so there may be additional 17.x-era connection requirements beyond the
  version floor; these are not needed for the classic wired/TCP HUP path the Vita uses.

## Recommended next steps

1. Spike (prototype experiment): change VERSION_REQUEST to `(1,7)` and confirm the
   2026 phone no longer closes the connection; capture the VERSION_RESPONSE bytes to
   record the exact reply (major/minor/status and any trailing bytes).
2. If `(1,7)` is rejected for any reason (unlikely), try `(1,2)` (open-headunit's
   value, reported working on AA 17.5) as a fallback before touching TLS/framing.
3. Record the captured VERSION_RESPONSE in `specs/research/aa-control-handshake.md`
   so the feature spec can pin the admission check (`status != 0xffff`) concretely.
4. Spec can rely on: format is raw 4-byte `major+minor`; target value `(1,7)`;
   response is ≥6 bytes `major+minor+status`. Still needs the live capture (step 1)
   to confirm the phone's exact reply length before finalizing the parser.
