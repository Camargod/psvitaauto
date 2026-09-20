# Research: aa-open-source-headunits
Date: 2026-09-15
Question: Which open-source "phone projection" head unit projects exist, and which components can we reuse or learn from for an embedded port?

## Findings

### 1. Project landscape (all public, all inspected on GitHub)

| Project | Language | License | Status | Stars | Role |
|---|---|---|---|---|---|
| f1xpl/openauto | C++/Qt5 | GPL-3.0 | Dormant (superseded by closed OpenAuto Pro) | ~2.9k | Reference head unit app (UI/audio/video/input) |
| f1xpl/aasdk | C++14 | GPL-3.0 | Dormant but complete | ~310 | Protocol library (transport, channels, handshake) |
| f1xpl/aasdk-proto | protobuf defs | GPL-3.0 | Merged into aasdk repo as `aasdk_proto/` | – | Message definitions |
| opencardev/crankshaft | Python/shell distro | GPL-3.0 | Semi-active | ~2.7k | RPi image builder bundling openauto+aasdk |
| openDsh/dash (+ forks of aasdk/openauto) | C++/Qt5 | GPL-3.0 | Semi-active | ~474 | Launcher/infotainment with wireless AA |
| viktorgino/headunit-desktop | C++/Qt5/QML | GPL-3.0 | Active-ish | ~325 | Car PC with AA client (aasdk submodule) |
| mikereidis/headunit | C | AGPL-3.0 | Dormant (2018) | ~254 | Early independent protocol implementation |
| andreknieriem/open-headunit | Kotlin (Android) | AGPL-3.0 | Very active (2026) | ~2.4k | AA head unit as Android app |
| uglyoldbob/android-auto (crate) | Rust | no license | Active, v0.3.8 | ~21 | Protocol crate (prost) |
| wiomoc/aa-player | Rust | no license | WIP (2025) | 2 | Head unit skeleton (tokio, rustls, nusb, cpal/gstreamer) |
| jwinarske/android-auto-client-rs | Rust | n/a | Dead (2021) | low | Early experiment |
| Go implementations | – | – | none found | – | Only Android *automation* tools exist (AutoGo etc.), no head unit protocol impl |
| Python implementations | – | – | none mature | – | Crankshaft image builder is Python; no protocol-level Python head unit found |

### 2. OpenAuto architecture (f1xpl/openauto, ~7.7k LOC)

- Two binaries: `autoapp` (main app) and `btservice` (separate process owning the Bluetooth
  channel). Structure: `autoapp/{UI, Configuration, Service, Projection}` + `btservice`.
- `Projection/` contains clean interfaces (`IVideoOutput`, `IAudioOutput`, `IAudioInput`,
  `IInputDevice`, `IBluetoothDevice`) with pluggable backends: `QtVideoOutput`,
  `OMXVideoOutput` (RPi hardware decode via ilclient/OpenMAX), `QtAudioOutput`,
  `RtAudioOutput` (RtAudio), `QtAudioInput`, `Local/Remote/DummyBluetoothDevice`,
  `InputDevice` (touch/button -> InputEvent).
- This interface layer is the most reusable design takeaway: protocol is fully decoupled
  from video/audio/input sinks.
- Video decode is RPi-specific (OMX); on other platforms Qt handles it in software.
- Wireless: connects as a **TCP client** to the phone's "head unit server"
  (`tcpWrapper_.asyncConnect(*socket, ipAddress, 5277, ...)` in `ConnectDialog.cpp`),
  enabled via Android Auto developer settings. Same HUP over TLS as USB, just different transport.

### 3. aasdk (f1xpl/aasdk, ~13.6k LOC C++ + 2.9k LOC protobuf)

- **Layers (src/):**
  - `Transport/` – `ITransport`, `USBTransport` (libusb-1.0), `TCPTransport`
    (boost::asio header-only), `SSLWrapper` (OpenSSL; wraps socket BIOs, `SSL_set_connect_state`,
    `SSL_VERIFY_NONE`, uses a locally generated self-signed certificate), `DataSink`.
  - `Messenger/` – framed message protocol: `MessageInStream`/`MessageOutStream` with
    `FrameHeader` (FrameType FIRST/CONSECUTIVE, SHORT/EXTENDED frame size, encryption type),
    `FrameSize`, `MessageIdGenerator`, per-channel promise queues. This is the AA framing layer.
  - `Channel/` – one class per channel: Control (incl. ServiceDiscovery, AV setup, version
    check, handshake/ping/audio focus), AV (video+audio focus, AVInput), Input
    (touch/button events), Sensor (GPS/gyro/etc.), Bluetooth. Callback interfaces
    (`IControlServiceChannelEventHandler`, etc.) decouple the library from the app.
  - `IO/` – thin wrappers (`IOContextWrapper`, `Promise`, `PromiseLink`) around
    **boost::asio::io_service** (header-only asio; only `boost::system` and `boost::log`
    are compiled deps).
  - `USB/` – AOAP (Android Open Accessory Protocol) handshake as a chain of queries
    (protocol version, vendor string, accessory start). Entirely unneeded for wireless-only.
  - `Common/` – `DataBuffer`, `ByteArray`, `Promise`, small crypto helpers.
- **Dependencies (CMakeLists):** Boost (system, log compiled; asio header-only), libusb-1.0,
  protobuf, OpenSSL. C++14. gtest for unit tests. ~10k LOC of the 13.6k is src (excl. tests).
- **aasdk-proto:** no standalone repo today; lives in `aasdk/aasdk_proto/`. 96 `.proto`
  files: **87 proto3 + 9 proto2** (legacy). Namespaced `f1x.aasdk.proto.*`. Generated C++
  via protoc into a static lib linked by both aasdk and openauto.
- Note: f1xpl/openauto also exists as an actively maintained fork under openDsh/openauto
  and aasdk under openDsh/aasdk (used by openDsh/dash, with wireless support).

### 4. Crankshaft

- Not a code library: a turnkey RPi image/distro (image_builder, CSOS config, shell+Python
  scripts) wrapping OpenAuto + aasdk + pulseaudio. Useful only as an integration reference
  (what a full stack needs: audio routing, autostart, wifi setup).

### 5. Rust/Go/Python implementations

- **`android-auto` crate (uglyoldbob, crates.io, v0.3.8):** pure-Rust protocol implementation
  (prost-generated messages), used by nothing significant yet, no license → reference only.
- **`aa-player` (wiomoc):** Rust WIP; stack = tokio, prost (+ local `.proto` files),
  rustls (with a patched webpki), nusb (USB transport), cpal + gstreamer (audio). Shows a
  modern, dependency-light pattern (prost + rustls + tokio) that maps well to embedded.
- **"aa-proto-rs" as such does not exist** on crates.io (404); the name appears only in
  informal references. The real equivalents are the `android-auto` crate and aa-player's
  vendored protos.
- **Go:** no head unit protocol implementation found on GitHub (only Android automation
  frameworks, unrelated).
- **Python:** no protocol-level head unit implementation found; Python appears only in
  Crankshaft's tooling.

### 6. Wireless vs USB support

- USB (AOAP): aasdk (libusb), openauto, headunit-desktop, open-headunit (Android accessory
  mode), aa-player (nusb), mikereidis/headunit.
- Wireless (Wi-Fi): OpenAuto (TCP client → phone head unit server, port **5277**, TLS);
  openDsh/dash (wireless capability); OpenAuto Pro (commercial, wireless + CarPlay).
  Wireless = same HUP/TLS stack over TCP; no AOAP needed.

### 7. Embedded (ARM Cortex-A9 / VITASDK) portability assessment

- **C++14 codebase**: compiles under arm-vita-eabi-g++ in principle (newlib 4.1, C++11/14/17
  supported per our vita-toolchain research).
- **Boost**: only `boost::system` + `boost::log` (compiled) + `boost::asio` (header-only)
  are used. asio on Vita would fall back to select() (no epoll); pthreads exist. Porting
  Boost to VITASDK is possible but heavy (~hours of build); boost::log is easily replaced
  by a custom logger; asio could be replaced by a hand-rolled event loop or libuv-like
  minimal loop. **Replaceable, not a blocker.**
- **OpenSSL**: TLS client with self-signed cert, `SSL_VERIFY_NONE`. Vita has mbedTLS
  (VitaSDK package) and OpenSSL ports; the AA handshake is plain TLS — either works.
  **Replaceable (mbedTLS).**
- **libusb**: only needed for USB transport. Wireless-only → **drop entirely**.
- **protobuf**: full libprotobuf C++ compiles for ARM/newlib but is heavy (~2–4 MB binary,
  long build). Alternatives: **protobuf-c** (C runtime) or **nanopb** regenerating C code
  from the same `.proto` files — much better embedded fit. Wire format is standard
  protobuf, so regenerating from aasdk_proto is safe.
- **Qt / RtAudio / OMX**: not portable to Vita; must be replaced anyway (Qt far too heavy).
  OpenAuto's `IVideoOutput`/`IAudioOutput`/`IInputDevice` interfaces are the exact seams we
  would implement against Vita hardware (SceAvcdec for H.264, sceAudio, vita2d/SDL for UI).

### 8. Protobuf details

- 96 proto files, 87 proto3 + 9 proto2 (both syntaxes must be handled by the generator —
  protoc 3.x handles both; nanopb/protobuf-c handle proto2/proto3 subsets, all our messages
  are simple enough).
- Proto source ~2.9k LOC → generated C++ roughly 3–5x that (~10k LOC); generated C
  (protobuf-c/nanopb) roughly 1–2x. Message sizes are tiny (setup/handshake/input events);
  only video channel payloads are byte streams carried *outside* protobuf (H.264 frames in
  the framing layer, not protobuf).

## Sources (URLs)

- https://github.com/f1xpl/openauto (README, CMakeLists, src tree; cloned at development branch)
- https://github.com/f1xpl/aasdk (README, CMakeLists, src tree, aasdk_proto/*.proto; cloned at development branch)
- https://github.com/f1xpl/aasdk/tree/development/aasdk_proto (file listing of 96 .proto files)
- https://github.com/opencardev/crankshaft (+ raw README)
- https://github.com/openDsh/dash ; https://github.com/openDsh/openauto ; https://github.com/openDsh/aasdk (via GitHub API org listing)
- https://github.com/viktorgino/headunit-desktop
- https://github.com/mikereidis/headunit
- https://github.com/andreknieriem/open-headunit
- https://github.com/uglyoldbob/android-auto ; https://crates.io/api/v1/crates/android-auto
- https://github.com/wiomoc/aa-player (+ raw Cargo.toml)
- https://github.com/jwinarske/android-auto-client-rs
- GitHub search APIs: repositories for "android auto headunit", language:rust, language:go, language:python
- https://crates.io/api/v1/crates/aa-proto-rs (404 — crate does not exist)

## Implications for PSVitaAuto

1. **The protocol layer is small and self-contained.** aasdk proves the whole head unit
   protocol (TLS handshake, framing, channel open/close, version check, input events,
   video/audio focus) is ~10k LOC of C++ excluding USB/AOAP and tests. An embedded
   reimplementation is realistic.
2. **Wireless-only dramatically shrinks the problem.** We need: TCP client → phone
   (port 5277) + TLS (mbedTLS) + AA framing + channels. No libusb, no AOAP, no USB gadget.
3. **Message definitions are directly reusable.** The `.proto` files in `aasdk_proto/`
   define the wire contract (GPL-3.0 — see licensing note below). We can regenerate C
   bindings with protobuf-c/nanopb for the Vita, or hand-roll the handful of messages we use.
4. **Video/audio/input seams are known.** OpenAuto's Projection interfaces map 1:1 onto
   Vita hardware: `IVideoOutput` → SceAvcdec + GXM/GPU textures; `IAudioOutput` → sceAudio;
   `IInputDevice` → ctrl/peek touch + buttons. AA video is H.264 (Vita has a hardware decoder).
5. **Licensing:** aasdk/openauto are GPL-3.0 — fine for homebrew, but copying code makes
   PSVitaAuto GPL-3.0. AGPL projects (mikereidis, open-headunit) impose network-source
   obligations; use them only as reading material. The `android-auto` Rust crate has **no
   license** — do not copy from it. Reimplementing from the (GPL) `.proto` definitions +
   behavior observations is the cleanest path; GPL-3.0 for the result is acceptable for a
   hobby project (decide in spec).
6. **The modern Rust pattern (prost + rustls + tokio) is not applicable on-device** (Rust
   toolchain for Vita is immature/absent), but it validates that a small TLS+framing core
   is all that's needed.

## Remaining unknowns

- Exact AA protocol version/feature flags currently negotiated by phones (the open-source
  implementations target AA 2018-era protocol; modern phones may require specific version
  strings — open-headunit Kotlin source is the best current reference; needs deeper reading).
- Whether Android Auto's head unit server mode still works on current phone Android
  versions and what certificate validation the phone performs (aasdk uses
  `SSL_VERIFY_NONE`; newer AA builds might have tightened this — OpenAuto Pro works today,
  so presumably still fine, but unverified).
- Video stream codec specifics over wireless (H.264 profile/level, bitrate caps, actual
  resolution Vita can sustain at its Wi-Fi throughput) — needs a dedicated research task
  (cross-check with vita-hardware.md findings).
- mbedTLS vs OpenSSL on Vita for the AA handshake (cipher suite requirements of the phone).
- protobuf-c/nanopb handling of the 9 proto2 files (syntax edge cases).
- Real LOC/effort of stripping boost::asio out of aasdk for a direct port (vs full reimplement).

## Recommended next steps

1. Pick the **reference implementation**: aasdk (protocol) + openauto (app seams) as primary
   reading; open-headunit (Kotlin) as the most current behavior reference.
2. Write a **protocol spec feature** (handshake sequence, framing format, channel lifecycle,
   input event encoding) derived from aasdk, with the `.proto` files vendored/regenerated
   for embedded C (protobuf-c or nanopb) — this becomes the core spec for implementation.
3. Decide **port vs reimplement** in that spec: recommended default is **reimplement in C++
   or C on VITASDK** (wireless TCP + mbedTLS + protobuf-c/nanopb + SceAvcdec), using aasdk
   as the golden reference; porting aasdk verbatim would drag in Boost/libusb/Qt-adjacent
   baggage with high porting cost for little gain.
4. Next research tasks: (a) AA video stream format + Vita decode feasibility, (b) mbedTLS
   TLS handshake interop with the phone's head unit server, (c) license decision for the
   project (GPL-3.0 vs clean-room).
