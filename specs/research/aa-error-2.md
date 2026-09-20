# Research: aa-error-2

Date: 2026-09-16
Question: what does Android Auto "communication error 2 (incompatible versions)" mean and what version is required?

## Findings

### TL;DR

"Communication error 2 — the smartphone and the car are running incompatible
software versions" is a **phone-side** (Android Auto app) user-facing error that
the phone shows when the GAL protocol **version negotiation fails** — i.e. the
very first protocol step, `VERSION_REQUEST`/`VERSION_RESPONSE`, before TLS even
starts. It is NOT a TLS/certificate rejection (that is a later step, surfaced as
different error numbers), and it is NOT caused by ServiceDiscovery (that happens
even later). The correct current version is still **major = 1, minor = 7**. No
additional admission step exists before or during `VERSION_REQUEST` on the TCP
"head unit server" path. Our harness already sends byte-correct `1.7`, so the
remaining work is to *prove*, with a phone logcat capture, which side is
rejecting the handshake rather than to keep guessing at the version number.

### 1. What "communication error N" is, and where it comes from

- The message is rendered by the **phone's** Android Auto app
  (`com.google.android.projection.gearhead`), not by the head unit. In developer
  "head unit server" mode the phone is the server, and when the GAL handshake
  fails it prints a numbered "Communication error N" on its own screen. (Source:
  user observation + the AA ecosystem's issue reports, below.)
- "Communication error N" is a **numbered family**, each number a distinct
  failure. Documented examples from primary sources:
  - **error 2** = "…running incompatible software versions" (this note).
  - **error 7** = "Security control" — open-headunit issue #252, which its
    maintainer attributed to wrong *video* settings (`h265` on an Android 4.4
    device); fixed by h264 / a newer release.
  - **error 8** = "make sure date and time are same and Google Play services is
    updated" — carstream-android-auto issue #204; this is certificate **clock
    skew** (`CERT_NOT_YET_VALID` / `CERT_EXPIRED`).
  - **error 10 / native 2** correlation — sn-00-x/aa4mg issue #11: an openauto
    log of `[AndroidAutoEntity] channel error: AaSdk error code: 10, native code:
    2` was reported alongside the phone's "Communication error 2 … incompatible
    software". (Note: aasdk error 10 is `USB_TRANSFER`, so in that USB/AOAP case
    "error 2" was a *transport* failure surfacing as the same generic screen —
    see "Remaining unknowns".)
- **Fact vs hypothesis:** that "error 2" belongs to a numbered phone-side error
  family and that 7/8 map to video/cert-clock respectively is source-backed. The
  *exact* phone-internal enum index behind the literal "2" is not pinned to a
  single authoritative public source (the AA APK is ProGuard/R8-obfuscated and
  its `strings.xml` is not in a public repo); the mapping below is the
  best-supported reconstruction.

### 2. Which wire step triggers "error 2": the VERSION handshake

The phone logs version negotiation under the `CAR.GAL.GAL.LITE` logcat tag
(openauto-prodigy `docs/aa-protocol/aa-phone-side-debug.md`, decompiled AA APK
v16.1):

```
CAR.GAL.GAL.LITE: Car requests protocol version v1.1
CAR.GAL.GAL.LITE: Negotiated protocol version v1.7 (STATUS_SUCCESS)
```

So the phone **does** decide compatibility at the `VERSION_REQUEST` step and
records a `STATUS_SUCCESS` or a failure. The phone-side status enum
(openauto-prodigy cross-reference + `apk-proto-reference.md`) is:

| value | name | meaning |
|---|---|---|
| 0 | STATUS_SUCCESS | |
| 1 | STATUS_UNSOLICITED_MESSAGE | |
| -1 | STATUS_NO_COMPATIBLE_VERSION | **version mismatch** |
| -2 | STATUS_CERTIFICATE_ERROR | TLS cert |
| -3 | STATUS_AUTHENTICATION_FAILURE | TLS auth |

The phone's "disconnect reasons" enum (`xkh`, same decompilation) also separates
version from config/auth:

| value | name | actionable |
|---|---|---|
| 1 | PROTOCOL_INCOMPATIBLE_VERSION | check version negotiation |
| 2 | PROTOCOL_WRONG_CONFIGURATION | fix ServiceDiscoveryResponse |
| 3 | PROTOCOL_IO_ERROR | check TCP |
| 7 | PROTOCOL_AUTH_FAILED | TLS handshake failed |
| 8 | PROTOCOL_AUTH_FAILED_BY_CAR | our cert rejected |
| 9 | TIMEOUT | |
| 13/14 | …CERT_NOT_YET_VALID / CERT_EXPIRED | clock skew |

**Conclusion for Q2:** "error 2 / incompatible software versions" correlates with
the **version-negotiation step**, i.e. `VERSION_REQUEST`→`VERSION_RESPONSE`, and
the wire-level signal is `VERSION_RESPONSE.status == 0xFFFF` (MISMATCH) /
`STATUS_NO_COMPATIBLE_VERSION (-1)`. It is **not** TLS cert rejection
(that is reason 7/8/13/14, status -2/-3, and shows as error 7/8, not 2), and
**not** ServiceDiscovery misconfiguration (reason 2, which is a different
failure class). The only caveat is that "error 2" can also be a generic
connection/transport failure in some paths (aa4mg), so it is a *strong* but not
*absolute* indicator of the version step — see §5 and "Remaining unknowns".

### 3. The correct version for current (2025–2026) head units

Primary-source version constants in active head units (re-verified this session):

| Project | language | major | minor | raw payload | activity | mode |
|---|---|---|---|---|---|---|
| f-io/LIVI `livi-aa` | Rust | 1 | **7** | `00 01 00 07` | 2026-09 | native wireless |
| mrmees/openauto-prodigy | C++ | 1 | **7** | `00 01 00 07` | 2026-09 | native wireless |
| andreknieriem/open-headunit | Kotlin | 1 | **2** | `00 01 00 02` | 2026-08 | developer head-unit server |
| f1xpl/aasdk | C++ | 1 | 1 | `00 01 00 01` | legacy | (stale) |
| uglyoldbob/android-auto | Rust | 1 | 1 | `00 01 00 01` | 2026-07 | (stale) |

- **LIVI** `consts.rs`: `VERSION_MAJOR = 1; VERSION_MINOR = 7; VERSION_STATUS_MISMATCH = 0xffff`.
- **open-headunit** `Messages.kt`: `private var VERSION_REQUEST = byteArrayOf(0, 1, 0, 2)` (i.e. **1.2**), sent as `createRawMessage(0, 3, 1, …)`.
- **Phone's own range** (decompiled AA APK v16.1, openauto-prodigy
  cross-reference): "Phone supports: v1.6 (minimum) → v1.7 (preferred); fallback
  cap v6.0"; the selectable "GAL" request values are exactly **1.7, 4.3, 5.0,
  5.1, 6.0** (4.3+ unlock H.265/overlays/MediaOptions; 1.7 is the legacy
  H.264 minimal profile).

**Conclusion for Q3:** there is **no 1.8 / 1.9 / 2.0**. `(1,7)` is the current
correct "legacy" value for a minimal H.264 video+input head unit, and it is what
the two most sophisticated active HUs (LIVI, Prodigy) send. `(1,2)` (open-headunit,
developer-mode reference) also still works on AA 17.5. `(1,1)` (aasdk) is stale.
The phone negotiates a *range* (min 1.6 → preferred 1.7), so 1.7 is inside the
accepted window; the value itself is not the likely cause of a rejection.

### 4. Is there a missing admission step before/during VERSION_REQUEST?

**No.** On the TCP "head unit server" path the sequence is exactly:

```
TCP connect (HU → phone:5277)
  → VERSION_REQUEST  (0x0001, PLAIN, control ch 0)   ← FIRST bytes on the wire
  → VERSION_RESPONSE (0x0002, PLAIN)
  → SSL_HANDSHAKE ×N (0x0003, PLAIN; TLS 1.2 records in-band)
  → AUTH_COMPLETE    (0x0004, PLAIN)
  → SERVICE_DISCOVERY_REQUEST/RESPONSE (0x0005/0x0006, ENCRYPTED)
  → CHANNEL_OPEN … → media/input
```

- There is **no** preamble, magic byte, or "hello" before `VERSION_REQUEST`
  on TCP. `aa-framing.md` §6 (aasdk `TCPTransport`/`TCPEndpoint`): "No pre-TLS
  plaintext preamble or magic bytes on the TCP socket (unlike USB/AOA mode…)".
- Service discovery is **after** TLS+auth, never before version.
- (For completeness: USB/AOA mode *does* send AOAP version strings first, but the
  developer head-unit-server mode we use is TCP and has no such preamble.)

### 5. Does the phone require a specific source / TLS cert *earlier* than version?

**No, and the ordering rules this out as a cause of "error 2".**

- The head unit's "Google Automotive Link" client certificate is presented
  **during `SSL_HANDSHAKE`**, which is *after* the plaintext version handshake.
  Therefore a wrong/missing cert cannot produce a *version* failure; it would
  surface later as auth failure (reason 7/8/13/14 → "error 7/8").
- The developer server does **not** require the HU to connect from a specific
  source; it binds LAN-reachable and accepts the HU as TCP client + TLS client
  (openauto `ConnectDialog.cpp`, open-headunit `NetworkDiscovery.kt`/`AapSslContext.kt`).
- Two phone-side *pre-session* gates exist but are phone-internal and produce
  their own behavior, not a version error: the connection state machine
  (`xje`: `WAIT_FOR_CAR_CONNECTION → CHECK_COUNTRY_WHITELIST →
  CHECK_PHONE_BLACKLIST → AUTHORIZE_CAR_CONNECTION → …`) and the
  "Add new cars to Android Auto" consent / unlocked-screen requirement. The
  server is also **single-connection** (open-headunit `UnresponsivePeerPolicy.kt`):
  a stranded half-open socket makes it deaf to later attempts until restarted.

### 6. Our harness already sends the correct bytes (code-verified this session)

- `src/core/control.c:74` calls `aa_version_encode_request(1, 7, …)` → major=1,
  minor=7 (big-endian) — the correct value.
- `src/core/session.c` + `src/core/frame.c` emit the full frame, which I traced
  to exactly `00 03 00 06 00 01 00 01 00 07` (channel 0, flags 0x03 BULK|PLAIN,
  length 6, msgid 0x0001, payload 0x0001 0x0007). This is byte-identical to
  LIVI's round-trip test (`frame.rs`: header `[00,03,00,06]`, type `00 01`,
  payload `00 01 00 07`).
- Caveat: the *spec* (`specs/features/aa-control-channel/spec.md:47`) and its QA
  report still say `major=1, minor=1`; the code was bumped to 1.7 without a
  matching spec/QA update. That is a process drift to reconcile, but the running
  source already sends 1.7.

### 7. So why would the phone still show "error 2"?

Given correct bytes and a correct version value, "error 2" is **not** explained
by the version number. The remaining candidate causes, in order of likelihood:

1. **Live-wire / transport problem the unit tests can't see** — e.g. the mock
   phone (`src/host/mock_phone.c`) shares the harness's framing, so tests can pass
   while a real phone sees a malformed frame (wrong length/flags on a *fragment*
   path, or a byte-order bug only hit live). A phone that cannot parse the first
   frame has no "compatible version" and reports version failure.
2. **VERSION_RESPONSE parsing on our side** — the Prodigy cross-reference notes
   the phone may reply with **4 shorts (`type, major, minor, status`)** while
   aasdk/LIVI read **3 shorts**. If we misread the reply length we could fail
   *after* the phone already logged a match, but this alone would not make the
   *phone* print "error 2".
3. **Phone-side admission differs in developer mode** vs the native path LIVI/
   Prodigy exercise — the developer-mode reference (open-headunit) uses 1.2, not
   1.7. Unverified whether 1.7 vs 1.2 changes anything on the *developer* server.
4. **Pre-session state** — phone locked, consent not accepted, or a stranded
   earlier connection (single-connection server) making the current attempt
   appear dead, surfaced as a generic "communication error".

## Sources (URLs)

- f-io/LIVI `livi-aa` version + status constants:
  https://github.com/f-io/LIVI/blob/main/native/livi-helperd/crates/livi-aa/src/consts.rs
  and `…/src/session.rs` (VERSION_REQUEST/`VERSION_STATUS_MISMATCH` handling)
- andreknieriem/open-headunit `Messages.kt` (`VERSION_REQUEST = byteArrayOf(0,1,0,2)`):
  https://github.com/andreknieriem/open-headunit/blob/main/app/src/main/java/com/andrerinas/openheadunit/aap/protocol/messages/Messages.kt
- f1xpl/aasdk `Version.hpp` (1.1), `ErrorCode.hpp` (enum incl. `USB_TRANSFER=10`), `Error/Error.cpp`:
  https://github.com/f1xpl/aasdk/blob/master/include/f1x/aasdk/Version.hpp
- mrmees/openauto-prodigy protocol cross-reference (version range 1.6→1.7, status enum, GAL values):
  https://github.com/mrmees/openauto-prodigy/blob/main/docs/aa-protocol/android-auto-protocol-cross-reference.md
- mrmees/openauto-prodigy APK deep-dive (disconnect reasons `xkh`, session errors `xki`, connection states `xje`):
  https://github.com/mrmees/openauto-prodigy/blob/main/docs/aa-protocol/aa-apk-deep-dive.md
- mrmees/openauto-prodigy APK proto reference (VERSION_REQUEST/RESPONSE raw layout, status enum):
  https://github.com/mrmees/openauto-prodigy/blob/main/docs/aa-protocol/apk-proto-reference.md
- mrmees/openauto-prodigy phone-side debug (`CAR.GAL.GAL.LITE` "Car requests protocol version / Negotiated protocol version"):
  https://github.com/mrmees/openauto-prodigy/blob/main/docs/aa-protocol/aa-phone-side-debug.md
- open-headunit issue #252 "Communication error 7 - Security control" (video-settings cause):
  https://github.com/andreknieriem/open-headunit/issues/252
- carstream-android-auto issue #204 "Communication Error 8" (date/time = cert clock skew):
  https://github.com/thekirankumar/carstream-android-auto/issues/204
- sn-00-x/aa4mg issue #11 (openauto `AaSdk error code: 10, native code: 2` ↔ "Communication error 2 … incompatible software"):
  https://github.com/sn-00-x/aa4mg/issues/11
- Local: `specs/research/aa-framing.md` (§6 no TCP preamble), `aa-control-handshake.md`
  (§1 version bytes, §2 order), `aa-current-version.md` (version table), `aa-head-unit-server.md`
  (§8 single-connection proxy)

## Implications for PSVitaAuto

1. **The version value is already right.** Do not chase 1.8/1.9/2.0 — none exist.
   `(1,7)` matches LIVI/Prodigy byte-for-byte, and `(1,2)` (open-headunit,
   developer-mode reference) is a valid fallback. Leave `control.c:74` at `(1,7)`.
2. **Reconcile the spec/QA drift.** Update `aa-control-channel/spec.md` and its QA
   record from `major=1, minor=1` to `major=1, minor=7` so the single source of
   truth matches the code (golden rule #2).
3. **Treat "error 2" as a *symptom*, not a verdict.** It means "version/connection
   negotiation failed", which with a correct 1.7 request points at framing,
   transport, or phone state — not the version number. The next step is evidence,
   not more guessing.
4. **Add a `CAR.GAL.GAL.LITE` logcat capture to the harness spike.** This is the
   single most decisive diagnostic: it prints exactly what version the phone
   parsed and the negotiation result (`STATUS_SUCCESS` vs
   `STATUS_NO_COMPATIBLE_VERSION`).
5. **Harden the VERSION_RESPONSE parser** for the 3-short vs 4-short ambiguity
   (`type, major, minor, status`) flagged in `aa-current-version.md`, so we don't
   misparse a reply the phone already accepted.

## Remaining unknowns

- **Exact phone-internal enum index behind the literal "error 2".** The message
  text ("incompatible software versions") is unambiguous, but the AA APK is
  obfuscated and its UI strings aren't in a public repo, so the precise
  `communication_error_2` → internal-reason mapping is reconstructed, not
  directly sourced.
- **Whether "error 2" can mask a non-version failure.** The aa4mg case showed a
  USB transport error surfacing as the same "error 2" screen; the boundaries of
  this generic behavior on the TCP developer-server path are unverified.
- **Developer-mode vs native-mode version enforcement.** The developer-mode
  reference (open-headunit) uses 1.2 while native HUs use 1.7; whether the
  developer server treats 1.7 differently is unverified. Needs a live test.
- **VERSION_RESPONSE exact reply length** (3 vs 4 shorts) on the user's AA
  version — needs the logcat/capture.

## Recommended next steps

1. **Spike (ground truth):** on the real phone, enable AA developer logging
   (`adb logcat`), start the head-unit server, run the harness, and capture
   `CAR.GAL.GAL.LITE` + `CAR.GAL.SECURITY.LITE`. The two lines
   "Car requests protocol version vX.Y" and "Negotiated protocol version …
   (STATUS_…)" will tell us whether the phone even parsed 1.7 and what it
   decided — resolving hypotheses §7.1/§7.3 in one shot.
2. **Also capture the raw socket bytes** the harness emits (tcpdump on the
   `adb forward` port or a hexdump in the harness) and diff against the known-good
   `00 03 00 06 00 01 00 01 00 07` frame to rule out a live-only framing bug.
3. **If the phone logs `STATUS_NO_COMPATIBLE_VERSION` with a correctly-parsed
   1.7**, try `(1,2)` (open-headunit's developer-mode value) as a fallback before
   touching TLS/framing.
4. **If the phone logs a match but the harness still fails**, the problem is
   downstream (VERSION_RESPONSE parsing or TLS); fix the 3/4-short parser and
   validate the SSL_HANDSHAKE loop against `aa-tls-mechanics.md`.
5. **Update the feature spec** to pin: version `(1,7)` (fallback `(1,2)`), the
   `status != 0xffff` admission check, and the required logcat evidence before
   the `aa-control-channel` feature is re-QA'd.
