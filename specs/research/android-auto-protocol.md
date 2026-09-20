# Research: android-auto-protocol
Date: 2026-09-15
Question: How is the Android Auto head unit communication protocol (HUP) documented in public sources, and what open-source implementations exist that we can reuse?

## Findings

### 1. What Google publicly documents vs. what it does not
- Google documents only the outer layers. The proprietary head unit protocol (HUP) itself (TLS, framing, protobuf message set, channel model) has **no official public spec**. Everything known about it comes from community reverse engineering preserved in open-source codebases (aasdk and its descendants) and from traffic captures.
- Publicly documented pieces:
  - **AOA (Android Open Accessory) protocol** on source.android.com (`/docs/core/interaction/accessories/protocol`, `/aoa`, `/aoa2`): the USB side of wired Android Auto. Accessory (head unit) is the USB host; handshake via vendor control requests (protocol version query, identification-string write, start accessory mode, audio-request in AOAv2); product IDs `0x2D00`/`0x2D01` (AOAv1), `0x2D04`/`0x2D05` (AOAv2, adds audio); two bulk endpoints carry the data. Verified against the archived AOAv2 page (PIDs confirmed).
  - **High-level wireless Android Auto existence** on developer.android.com (AA projection on AAOS head units, BT-pairing + Wi-Fi handover) — page fetch failed from this environment; the practical wireless flow is instead documented by the open-source implementations below.
- Everything else below (TLS details, frame format, message IDs, channel model, port 5277) is sourced from public open-source code, not from Google docs.

### 2. Protocol stack end to end (as implemented by aasdk/openauto)
- **Transport**: either USB (AOA bulk endpoints, via libusb, host side) or TCP (wireless mode). In wireless mode the session runs over TLS on **TCP port 5277** (openauto `ConnectDialog.cpp` connects to the phone's IP on 5277; in "head unit server" mode the HU listens and the phone connects). Media data (video/audio) and control share this single connection.
- **TLS**: the head unit authenticates as a TLS **client with a fixed, self-signed X.509 client certificate** whose hardcoded cert + RSA private key ship inside aasdk (`src/Messenger/Cryptor.cpp`; certificate subject org "Google Automotive Link"). TLS 1.2 via OpenSSL. The phone (TLS server) accepts this certificate in practice on stock Android Auto apps — openauto and forks have worked this way for years.
- **Framing** (aasdk `Messenger/FrameHeader.*`, `FrameSize.*`): each frame begins with a 2-byte header — byte 0 = channel ID, byte 1 = flags (frame type BULK/FIRST/MIDDLE/LAST for fragmentation, encryption flag, CONTROL/DATA message-type flag) — followed by a length field (1-byte SHORT or 4-byte EXTENDED that also carries the total message size). `MessageInStream`/`MessageOutStream` reassemble multi-frame messages.
- **Message payload**: protobuf messages. Each channel has its own message-ID enum (`*MessageIdsEnum.proto`), so a message is <channel ID, message ID, protobuf bytes>.
- **Multiplexing ("channel" concept)**: one connection carries many logical channels, each opened with a `ChannelOpenRequestMessage` carrying a `ChannelDescriptorData` and ACKed with `ChannelOpenResponseMessage`. Channels in aasdk: Control, Video, Media audio, System audio, Speech audio, Audio input (mic), Bluetooth, Sensor, Input, plus Navigation/MediaStatus in newer ports. This is exactly the "video, audio, input channels" concept the question asks about.
- **Control channel sequence**: VersionRequest/VersionResponse (protocol version 1.6 in aasdk), service discovery, channel open, `HandshakeComplete`, ping/pong keepalive (openauto `Pinger`), AuthComplete, FocusRequest (audio focus). Without a correct control channel the phone will not open video/audio.
- **Media formats**: video channel carries H.264 NAL units (phone encodes to the HU-requested resolution/FPS from `VideoFPS`/`VideoResolution` protos: 480p/720p/1080p at 30/60 fps); audio channels carry 16-bit PCM (48 kHz stereo media, 16 kHz speech, etc.); the HU opens/closes audio channels via AVChannelSetup messages. Input channel sends `TouchEvent`/`ButtonEvent`/`AbsoluteInputEvent` protos (see `hu/hu.proto` in gartnera/headunit: `TouchInfo` press/release/drag with multi-pointer locations, `ButtonInfo` scan codes, `InputEvent` with timestamps).

### 3. Wireless projection flow (phone → head unit over Wi-Fi)
- Canonical flow, as implemented by WirelessAndroidAutoDongle (MIT, C++), LIVI (GPL-3.0, Rust), openauto (GPL-3.0), and the Rust crate `android-auto` (LGPL):
  1. HU advertises over **Bluetooth** as a car kit (A2DP + HFP service records); the phone pairs with it (dongle name style `WirelessAADongle-*`).
  2. Over an **RFCOMM/SPP channel** the phone and HU exchange pairing/handoff info (BluetoothPairingRequest/Response protos; `NetworkInformation` — the Wi-Fi SSID/PSK/address the phone must use; `BluetoothInformation` — HU BT MAC).
  3. The phone joins the HU's **Wi-Fi network** (HU-hosted AP or Wi-Fi Direct).
  4. The TLS session then starts over TCP 5277 and the AA protocol runs exactly as in wired mode.
- Key architectural fact for us: the TLS/framing/message layer is **identical for wired and wireless** — WirelessAndroidAutoDongle literally bridges a wireless phone session into a car's wired USB port, proving the layers are interchangeable. The physical medium is the only difference.
- Note: the standard phone-side trigger is the Bluetooth handoff. WirelessAndroidAutoDongle and LIVI both require a real BT radio advertising the car-kit profiles; LIVI's docs note the phone only starts a wireless session "over an HFP connection".

### 4. Public protobuf definitions found
- **f1xpl/aasdk, `aasdk_proto/` directory** — the canonical set: ~100 `.proto` files (AVChannel*, Audio*, Bluetooth*, Video*, Input* (TouchEvent/ButtonEvent), Sensor*, Control* message-ID enums, ChannelDescriptor, Ping, VersionRequest/Response, etc.). There is **no standalone "aasdk-proto" repo** on GitHub (searched; the protos live inside aasdk). GPL-3.0.
- **gartnera/headunit, `hu/hu.proto`** — wireless-era protos: `TouchInfo`, `ButtonInfo`, `InputEvent`, `BindingRequest/Response`, plus WiFi info messages. AGPL-3.0. This fork switched from hand-parsing to libprotobuf.
- **uglyoldbob/android-auto, `protobuf/` directory** (Rust crate) — `Bluetooth.proto`, `Wifi.proto` + the standard channel message set, LGPL-3.0-or-later.
- "github.com/Eselter/headunit-protocol" from the research brief **does not exist** (404; Eselter's repos are AA phone-side tools, unrelated). The real proto sources are the three above.

### 5. Open-source head unit implementations and status (as of 2026-09)
| Project | Lang / platform | License | Status |
|---|---|---|---|
| f1xpl/aasdk | C++14 lib (boost, libusb, protobuf, OpenSSL) | GPL-3.0 | Dormant since 2023-07, 310 stars; de-facto reference |
| f1xpl/openauto | C++/Qt5, Raspberry Pi | GPL-3.0 | Dormant since 2024-12, 2.9k stars |
| openDsh (org): dash + forks of aasdk/openauto | C++/Qt | GPL-3.0 | Last push 2024-10, 474 stars. (The "OpenDsh (Rust)" from the brief is a misnomer — OpenDsh is C++/Qt) |
| opencardev/crankshaft | Distro (Python build system) wrapping openauto+aasdk for RPi | GPL-3.0 | Active (2026-09), 2.7k stars |
| mikereidis/headunit | C (original Android headunit app) | AGPL-3.0 | Abandoned 2018, 254 stars |
| gartnera/headunit (fork of spadival/headunit) | C++/JNI, Mazda CMU (ARM), libprotobuf | AGPL-3.0 | Dormant ~2020, 355 stars; vendored protobuf-2.6.1-arm |
| tomasz-grobelny/AACS | C++/GStreamer (Android Auto *server* for AAOS/GenIVI) | GPL-3.0 | Dormant since 2024-02, 358 stars |
| nisargjhaveri/WirelessAndroidAutoDongle | C++ + buildroot, Pi Zero 2W etc. | MIT (vendors GPL aasdk) | 2.6k stars, last push 2025-07 — best wireless-flow reference |
| f-io/LIVI | Rust + TS/Electron + GStreamer, Linux/macOS | GPL-3.0-or-later | **Very active (2026-09)**, 713 stars; wired + wireless AA, HW video decode |
| uglyoldbob/android-auto | Rust crate `android-auto` 0.3.3 (crates.io), tokio/rustls | LGPL-3.0-or-later | Active (2026-07), clean trait API, wireless via bluetooth-rust |
| jwinarske/android-auto-client-rs | Rust | MIT | Abandoned 2021, stub only |
| andreknieriem/open-headunit | Kotlin (Android tablet as HU) | AGPL-3.0 | Active (2026-09), 2.4k stars |
| andyching168/HeadunitPad | Swift (iPad as AA display) | AGPL-3.0 | 2026, small |
| Headunit Reloaded (B3IT/Emil Borconi) | Android app | **Closed-source commercial** | Not reusable |

### 6. License implications for reuse
- **GPL-3.0** (aasdk, openauto, crankshaft, openDsh, AACS, LIVI): linking/copying forces PSVitaAuto to be GPL-3.0 too. Acceptable for Vita homebrew (VitaShell, vita-moonlight etc. are GPL), but it taints the whole codebase.
- **AGPL-3.0** (mikereidis/gartnera headunit, open-headunit, HeadunitPad): network copyleft; fine to *read* for wire-format knowledge, avoid copying code into a differently-licensed app.
- **LGPL-3.0-or-later** (uglyoldbob/android-auto Rust crate): the most permissive *library* option — usable from any app. Blocked for us by Rust-on-Vita toolchain maturity (see implications).
- **MIT** (WirelessAndroidAutoDongle): the glue is MIT, but it vendors aasdk (GPL), so the repo as a whole is effectively GPL.
- **Practical reading**: the `.proto` schemas and the observed wire format are interface information; the GPL risk comes from copying C++ implementation. A clean-room C implementation of the messenger/transport layers written from the schemas + aasdk behavior avoids the GPL issue entirely and is the recommended route; alternatively accept GPL-3.0 and port aasdk directly (less work, cleaner attribution).

### 7. Build dependency footprint of aasdk (C++ core) for embedded ARM
- `CMakeLists.txt` (aasdk, development): CMake ≥ 3.5.1, C++14, `-fPIC`. Hard deps: **Boost** (components system + log — mostly header-only usage), **libusb-1.0** (only for USB transport — droppable for a wireless-only build), **Protobuf** (C++ runtime; generated code compiles against protobuf 2.6+ and 3.x), **OpenSSL** (TLS 1.2 client with client cert).
- openauto *additionally* needs Qt5, RtAudio, a Bluetooth service daemon, and OpenMAX IL (RPi-specific) — openauto itself is **not portable to the Vita**; only aasdk (or a reimplementation of its ~4 layers: transport / SSL / messenger-framing / protobuf channels) is.
- **ARM cross-compilation precedent**: gartnera/headunit ships `mazda/protobuf-2.6.1-arm` (protobuf 2.6.1 cross-compiled for the Mazda CMU's ARM SoC); WirelessAndroidAutoDongle builds the whole stack for Pi Zero 2 W (ARM Cortex-A53) via buildroot. ARMv7 Cortex-A9 (Vita) is the same work class: boost.system is trivial, protobuf runtime ~1-2 MB, OpenSSL ~2-3 MB — total static libs well under 10 MB, negligible against the Vita's ~224 MiB userland budget.
- CPU budget on the Vita (4x A9 @ 333-444 MHz, NEON): TLS 1.2 handshake is a one-off ~tens of ms; per-frame work is framing + protobuf decode of small messages only (video bytes are pass-through to the hardware AVC decoder). Feasible; mbedTLS/OpenSSL-on-Vita NEON precedent exists in the homebrew ecosystem (vita-moonlight uses TLS for GameStream).

## Sources (URLs)
- AOSP, Android Open Accessory protocol docs (verified via archive.org; AOA + AOAv2, PIDs 0x2D00/0x2D01/0x2D04/0x2D05): https://source.android.com/docs/core/interaction/accessories/protocol and .../accessories/aoa2
- Google, Android Auto developer docs (wireless projection on AAOS; fetch failed from this environment, flow cross-checked against implementations): https://developer.android.com/training/cars/android-auto
- f1xpl/aasdk (transport, TLS+cert, framing, channels, ~100 proto files): https://github.com/f1xpl/aasdk — key files: src/Messenger/FrameHeader.cpp, FrameSize.cpp, Cryptor.cpp (hardcoded cert), src/Transport/*, src/Channel/*, aasdk_proto/*.proto, CMakeLists.txt
- f1xpl/openauto (port 5277 in src/autoapp/UI/ConnectDialog.cpp; services incl. Pinger): https://github.com/f1xpl/openauto
- gartnera/headunit (hu/hu.proto input messages; protobuf-2.6.1-arm): https://github.com/gartnera/headunit
- nisargjhaveri/WirelessAndroidAutoDongle (wireless flow: BT pairing → Wi-Fi → passthrough): https://github.com/nisargjhaveri/WirelessAndroidAutoDongle
- f-io/LIVI (modern Rust+TS HU; wireless AA requires BT HFP handoff + own AP): https://github.com/f-io/LIVI
- uglyoldbob/android-auto (Rust crate, LGPL-3.0-or-later, full stack description): https://github.com/uglyoldbob/android-auto ; https://crates.io/crates/android-auto ; https://docs.rs/android-auto
- jwinarske/android-auto-client-rs (MIT, dormant): https://github.com/jwinarske/android-auto-client-rs
- openDsh org (dash app + aasdk/openauto forks): https://github.com/openDsh/dash , https://github.com/openDsh/aasdk
- opencardev/crankshaft (distro): https://github.com/opencardev/crankshaft
- mikereidis/headunit: https://github.com/mikereidis/headunit
- tomasz-grobelny/AACS: https://github.com/tomasz-grobelny/AACS
- andreknieriem/open-headunit: https://github.com/andreknieriem/open-headunit
- andyching168/HeadunitPad (iPad-as-display precedent, closest analog to PSVitaAuto): https://github.com/andyching168/HeadunitPad
- harryjph/android-auto-headunit: https://github.com/harryjph/android-auto-headunit
- GitHub API search results used for status/star counts (2026-09-15)

## Implications for PSVitaAuto
- **Wireless-only, USB-free build**: the Vita cannot act as an AOA USB host to the phone, so we only need the TCP transport branch of the stack. That removes libusb and the entire AOA query machinery — a meaningful simplification.
- **Reusable pieces**: (a) the protobuf schemas (aasdk_proto) as the message contract; (b) the framing format (2-byte header + short/extended length) — trivial to reimplement; (c) the hardcoded client cert/key from aasdk (public in the repo; also present in every fork — phones accept it); (d) the channel open/control-channel sequence as a reference state machine; (e) the input message shapes from hu.proto (maps directly onto SceTouch/SceCtrl reports).
- **License decision**: porting aasdk wholesale ⇒ GPL-3.0 for PSVitaAuto (normal for Vita homebrew); clean-room C reimplementation of messenger/transport against the schemas ⇒ any license. Recommend: clean-room C core with aasdk as reference + schemas copied (schemas only), keeping PSVitaAuto's license choice open. The Rust LGPL crate is attractive but rust-vita tooling is not production-ready (flag for later re-check).
- **Critical architectural risk — wireless bootstrapping**: per vita-hardware.md the Vita has (1) no Wi-Fi AP mode, (2) Bluetooth 2.1+EDR with no usable userland SPP/HFP service records, and wireless AA is normally started by a BT handoff where the phone expects a car kit advertising HFP. This is THE blocker to solve in a spike: options are (a) phone-side helper that starts AA's wireless server without BT (rooted phone / existing AA wireless toggle / Headunit-Reloaded-style launcher app), (b) a kernel-level Wi-Fi AP plugin on the Vita (Marvell driver work), (c) phone hotspot as AP + Vita as client and trigger the session from the phone side. The protocol core itself is identical in all cases.
- **Media pipeline fit**: request 800x480 or 1280x720@30 H.264 from the phone (VideoResolution/VideoFPS protos) → hardware AVC decode (proven moonlight pipeline, decode-to-GXM-texture) → 960x544 blit; audio 48 kHz S16 → SceAudioOut MAIN port 1:1; input via TouchEvent (multi-touch press/drag/release) + ButtonEvent (d-pad/face buttons → AA scan codes).
- **CPU/memory**: TLS (one-time handshake) + framing + protobuf + TCP on one A9 core at 444 MHz is well within budget; total added static libs < 10 MB.

## Remaining unknowns
- Exact phone-side acceptance conditions for a wireless session without a Bluetooth car-kit handoff (what exactly triggers the phone to open/listen on TCP 5277; whether a companion Android app or `dumpsys`/settings toggle can do it on a stock phone). Highest-priority unknown; determines the whole connectivity architecture.
- Whether Vita homebrew can create a Wi-Fi AP or Wi-Fi Direct group at all (driver/kernel plugin feasibility — carried over from vita-hardware.md).
- Protocol drift: which Android Auto phone versions still speak the aasdk-era message set (version 1.6) unmodified — aasdk-era protos are ~2018-2023; recent AA releases may have added/changed messages. Must validate against a current phone early.
- Legal gray zone: shipping the reverse-engineered Google Automotive Link cert/key and protobuf schemas in a public repo (all open-source HUs already do; risk is low but nonzero, and Google could change phone-side cert pinning in the future).
- Rust-on-Vita status (whether the LGPL crate could eventually be linked natively) — not checked in this pass.
- Audio codec variant actually negotiated (media 48 kHz stereo PCM vs AAC) and the exact AudioConfiguration proto fields for modern phones.

## Recommended next steps
1. **Dev harness on desktop first**: build openauto/aasdk on a Linux box (or run LIVI) against a real Android phone, capture a full session with Wireshark/TCPdump — gives us ground-truth message traces of the modern phone-side protocol before any Vita work.
2. **Spike the wireless bootstrap** (the make-or-break item): test phone-as-hotspot + HU-as-TCP-client with the BT handoff replaced by a phone-side trigger (AA wireless developer toggle, AAAD-style helper, or rooted `am` start), using openauto's head-unit-server mode as the reference. Decide topology: phone hotspot vs Vita AP plugin vs middleman dongle.
3. **Port the core, not the UI**: write/port a minimal C/C++ "aa-core" for vitaSDK: TCP socket → TLS client (OpenSSL Vita port, embedded cert) → frame reassembly → protobuf for the 5 channels we need (Control, Video, MediaAudio, SystemAudio, Input). Skip Bluetooth/Sensor/Navigation channels initially. GPL-3.0 if porting aasdk code, clean-room otherwise.
4. **Vita app integration**: H.264 → SceAvcdec → vita2d texture (moonlight pipeline), SceAudioOut MAIN for PCM, SceTouch/SceCtrl → TouchEvent/ButtonEvent protos; request 720p30 or 960x544 from the phone.
5. **Validation matrix**: 2-3 Android versions × 2-3 phone brands for the TLS/framing compatibility (protocol drift is real), then measure end-to-end latency (input→video) with an existing moonlight-style measurement.
