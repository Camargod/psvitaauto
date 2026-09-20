# Research: aa-head-unit-server

Date: 2026-09-16
Question: how to run the Android Auto "head unit server" on a real Android phone so a custom head unit (macOS harness / PS Vita) can connect to it, wired and wirelessly

## Findings

### 0. Terminology — there are TWO different "who listens on 5277" models (get this right first)

Android Auto has two distinct wireless topologies, and they are easy to conflate. The port
`5277` is used by both, but the *direction* of the TCP dial flips depending on the mode:

| Mode | TCP listener (binds 5277) | TCP client (dials out) | TLS server | TLS client |
|---|---|---|---|---|
| **Developer "Head unit server"** (what we want) | **Phone** | **Head unit** | Phone | Head unit (fixed cert) |
| **Native wireless AA** (production cars / dongles) | **Head unit** | **Phone** | Phone | Head unit (fixed cert) |

Key invariant: **the phone is always the TLS server and the head unit always presents the fixed
"Google Automotive Link" self-signed client certificate** (aasdk `src/Messenger/Cryptor.cpp`,
open-headunit `AapSslContext.kt` line 142 `createSSLEngine("android-auto", 5277)`). Only the TCP
listener side changes. In developer "head unit server" mode the *phone* binds `5277` and the *head
unit dials out to it — this is exactly the model DHU and openauto use. In native wireless the head
unit binds and the phone dials (open-headunit `WppTcpServer.kt` line 25-36: "this head unit
answering the phone's dial"; its comment on line 70-71 names `5277` as "Android Auto's own head
unit server that mode 1 dials outward"). Source-backed fact.

For PSVitaAuto we want the **developer "head unit server" mode**: the Vita/harness is a TCP client
that connects to `phone_ip:5277`, then does a TLS client handshake with the embedded cert.

### 1. Enabling Android Auto developer mode + "Start head unit server" (official)

Google's own DHU documentation (`developer.android.com/training/cars/testing` and `.../testing/dhu`)
documents the full flow:

1. Prerequisite: a device running **Android 9 (API 28) or higher**, with on-device **developer
   options** enabled.
2. Install **Android Auto** (`com.google.android.projection.gearhead`); update to latest.
3. Enable **Android Auto's own developer mode** (separate from the OS developer options):
   - Android 10 (API 29)+: **Settings > Apps & notifications > See all apps > Android Auto >
     Advanced > Additional settings in the app**.
   - Android 9 or lower: open the AA app menu → **Settings**.
   - Go to **About** near the bottom, tap **Version** to show "Version and permission info", then
     **tap that section 10 times**. An "Allow development settings?" dialog appears → **OK**.
   - Community docs (open-headunit, dmachard, joeeey) describe the same as "tap **Version** 10
     times". One-time only.
4. In the **overflow (three-dot) menu**, select **Start head unit server**. A persistent
   foreground-service notification appears ("head unit server is running").
5. Verify **Settings > Previously connected cars → Add new cars to Android Auto** is enabled.

Sources: developer.android.com/training/cars/testing#developer-mode and .../testing/dhu;
open-headunit wiki (Wireless + Troubleshooting); joeeey.com blog; dmachard.github.io post.

### 2. The DHU connection and exact `adb` commands (official)

DHU (`desktop-head-unit`, installed to `SDK_LOCATION/extras/google/auto/`) supports two transports:

- **ADB tunneling** (the one relevant to us):
  1. Start head unit server on the phone (above).
  2. Connect phone via USB, keep screen unlocked.
  3. `adb forward tcp:5277 tcp:5277`
  4. Run `./desktop-head-unit` (defaults to connecting to `127.0.0.1:5277`; the `--adb=<[localhost:]port>`
     flag overrides, default port 5277, default host localhost).
- **Accessory Mode (AOAP)** via `./desktop-head-unit --usb` — USB host mode, not applicable to the
  Vita and not needed for wireless.

`adb forward tcp:5277 tcp:5277` means: listen on **host** `127.0.0.1:5277` and tunnel to **device**
`5277` (where the AA head unit server is listening). The DHU is just a TLS-over-TCP client against
that tunneled socket. Source: developer.android.com/training/cars/testing/dhu.

### 3. Wireless — does the head unit server listen on the WiFi interface, and is root needed?

**Yes, the developer head unit server binds such that it is reachable over the phone's WiFi
interface on port 5277, on a stock (unrooted) phone.** Evidence:

- openauto (`ConnectDialog.cpp`) connects as a TCP client to an arbitrary `ipAddress` on port
  `5277` — the "wireless" path is literally "type the phone's LAN IP, HU dials `phone:5277`".
- open-headunit discovers the server over the LAN: `NetworkDiscovery.kt` probes candidate IPs on
  port `5277` (`checkPort(ip, 5277)`) and on success calls `commManager.connect(ip, 5277)`; its
  tests use `192.168.41.113:5277`. It also advertises the server over NSD/mDNS.
- joeeey.com documents connecting a second phone "via Wifi to your initial device running Android
  Auto" using only the head unit server.

**No root is required.** The developer toggle works on stock firmware. **No Wi-Fi Direct is
required** for this mode — the phone and head unit just need mutual IP reachability (same LAN).
Two caveats from open-headunit's Troubleshooting wiki:
- A **phone acting as the hotspot (10.x.x.x) has routing bugs** and "manual connection (port 5277)
  often fails" — prefer both devices on a shared external router/AP.
- Disable phone "Wi-Fi assistant / switch between networks / network acceleration" features, which
  kill the connection because the network has no internet.

The historical *helper-app* (`Wireless Helper`) existed only to **auto-trigger** the connection
without tapping the toggle; it is not needed for the manual developer-server path (and see §5: it
no longer works on AA 17.4+ anyway). WirelessAndroidAutoDongle and LIVI use the *native* wireless
flow (Bluetooth handoff → phone joins the HU's AP → phone dials the HU), which is a different
mechanism and requires the head unit to run an AP + Bluetooth car-kit service records — not what
the "head unit server" developer mode does.

### 4. The `adb`-based approach for a plain TCP client (our harness)

Yes — DHU's ADB-tunneling mode is nothing more than a plain TLS-over-TCP client connecting to
`127.0.0.1:5277` after `adb forward`. A custom client (macOS harness, and later the Vita) does the
exact same thing: run `adb forward tcp:5277 tcp:5277`, then `connect("127.0.0.1", 5277)` and begin
the TLS client handshake with the embedded certificate. There is no DHU-specific protocol before
TLS; DHU is a convenience front-end, not a gateway. (Fact, inferred from the DHU docs + the
identical TLS client role in openauto/aasdk/open-headunit.)

### 5. Android Auto version considerations (modern Android 13/14/15; does it still exist?)

- Android Auto is **built into Android as a system app** on Android 10+ (`com.google.android.projection.gearhead`).
  On Android 12+ the standalone phone-screen "Android Auto" launcher icon was removed, but the
  **Android Auto settings screen remains**, reachable at **Settings > Connected devices > Android
  Auto** (open-headunit wiki) or **Settings > Apps > Android Auto > Additional settings in the app**
  (official). On newer builds the version you tap 10× is shown under **Settings → Connected devices
  → Android Auto → Version** (or the "Version and permission info" stub at the bottom).
- The **"Start head unit server" developer option still exists and has not been renamed or removed**
  as of 2026 (open-headunit's actively-maintained README/wiki repeatedly describe it as the current
  path; Google's DHU docs still document it).
- **Major 2026 caveat:** **Android Auto 17.4+ broke almost all third-party wireless auto-triggers**
  (Self-Mode and the `Wireless Helper` app). open-headunit README states the **"Headunit Server
  (Developer Mode) is the only remaining solution for Self-Mode"** on AA 17.4+, and for wireless it
  lists USB dongle / Native Mode / Headunit Server. Implication: the developer "Start head unit
  server" toggle is not just an option, it is currently the *primary* reliable manual trigger.
- The exact AA app version is visible in the same "Version" screen; versions in the 17.x range are
  current in 2026. (Fact: open-headunit changelog/README; DHU official docs.)

### 6. Commands to force the server to start / listen on WiFi

- **Supported, stock, no root:** the UI toggle **Start head unit server**. There is **no publicly
  documented `am startservice` / `cmd` one-liner** to start Google's AA head unit server on a
  *stock* phone — the server lives in Google's `com.google.android.projection.gearhead`
  projection/car service and the component is not a public API. (Fact: absence of any such command
  across the official docs and the open-source HU ecosystem, which all use the toggle.)
- **Root (hypothesis, unverified):** on a rooted phone the service could in principle be started via
  `am startservice`/`cmd` against the gearhead package, but no public source documents the exact
  component name, so treat this as a spike item, not a fact.
- **Head-unit-side automation (not phone-side):** open-headunit exposes an intent surface
  (`headunit://connect?ip=<PHONE_IP>`, and a broadcast `ACTION_CONNECT` with `ip`) — but these run
  on the *head unit*, telling it which phone IP to dial. They are not a phone-side server trigger.
- The phone does **not** need `adb` once the server is running; `adb` is only needed for the USB
  forwarding path (§2/§4) or to read the phone's WiFi IP (`adb shell ip addr show wlan0`).

### 7. What the phone shows when a head unit connects; resolution

- **First connect:** the phone shows an on-device **terms-of-service / confirmation** to accept, and
  may request permissions (DHU docs step 8; also the "Add new cars to Android Auto" toggle). The
  phone must be **unlocked** for the DHU/session to launch.
- **While running:** the phone keeps the **"head unit server is running" foreground notification**;
  when actually projecting, the phone screen typically blanks/shows a minimal "Android Auto is
  connected" state (audio/UI go to the head unit). (Fact for the notification; the projection
  screen behaviour is the community-observed norm.)
- **Resolution is head-unit-requested, not phone-chosen.** AA supports exactly three video
  resolutions: **480p (800×480, default), 720p (1280×720), 1080p (1920×1080)**; the head unit asks
  for one via the `VideoResolution`/`VideoFPS` protos, and the DHU expresses this in
  `~/.android/headunit.ini` (`resolution`, `dpi`, `framerate`, margins). For the Vita (960×544) we
  request **720p or 480p** and letterbox/scale — 1080p is not useful and costs bandwidth. Source:
  developer.android.com/training/cars/testing/dhu "Video configuration".

### 8. Operational gotcha — the head unit server is single-connection

open-headunit's `UnresponsivePeerPolicy.kt` documents that the phone's 5277 server is "a proxy, not
an AAP server: it accepts, hands the socket to its own car service over binder, and waits there with
no timeout. Its accept loop is serial" — i.e. it serves **one** connection at a time, and a
connection that is opened but never completes the TLS/handshake leaves the server "deaf" to all
later attempts until the user **restarts the head unit server or kills Android Auto**. Practical
rule for our harness: **connect exactly once and follow through**; do not do speculative probe
connects to `phone:5277` (open-headunit's discovery probes are TCP connects that can strand sockets
— they had to add backoff logic specifically because of this). Source-backed fact.

## Sources (URLs)

- Google, "Test Android apps for cars" (developer mode enable steps): https://developer.android.com/training/cars/testing#developer-mode (fetched 2026-09-16 via archive.org/r.jina.ai; live fetch blocked from this env)
- Google, "Test using the Desktop Head Unit" (DHU install/run, adb forward 5277, --adb flag, video config 480p/720p/1080p): https://developer.android.com/training/cars/testing/dhu
- f1xpl/openauto — `src/autoapp/UI/ConnectDialog.cpp` (TCP client dials `ip:5277`): https://github.com/f1xpl/openauto/blob/master/src/autoapp/UI/ConnectDialog.cpp
- f1xpl/aasdk — `src/Messenger/Cryptor.cpp` (embedded "Google Automotive Link" client cert): https://github.com/f1xpl/aasdk
- andreknieriem/open-headunit — README (AA 17.4+ breaks wireless triggers; Headunit Server steps), wiki/Wireless, wiki/Troubleshooting (hotspot routing bug, loopback workaround), and source: `NetworkDiscovery.kt` (port 5277 probe/connect), `AutomationCommandPolicy.kt` (HEADUNIT_SERVER_PORT=5277), `AapSslContext.kt` (TLS engine "android-auto:5277"), `UnresponsivePeerPolicy.kt` (single-connection proxy), `WppTcpServer.kt` (native-mode phone-dials-HU), `contract/README.md` (ACTION_CONNECT ip→5277): https://github.com/andreknieriem/open-headunit
- nisargjhaveri/WirelessAndroidAutoDongle — README (native wireless via BT pairing + HU AP; no phone app needed): https://github.com/nisargjhaveri/WirelessAndroidAutoDongle
- f-io/LIVI — README (native wireless AA requires HU AP + BT HFP handoff; Android Auto wireless on Linux only): https://github.com/f-io/LIVI
- joeeey.com — "Reviving Android Auto for phone screens" (enable dev mode, Start head unit server, connect second phone via WiFi): https://joeeey.com/blog/reviving-android-auto-for-phone-screens/
- dmachard.github.io — "Android Auto Dev" (sdkmanager, Start Head Unit Server, `adb forward tcp:5277 tcp:5277`, run DHU): https://dmachard.github.io/posts/0068-android-auto-dev/

## Implications for PSVitaAuto

1. **Connectivity model is settled.** For our harness/Vita the phone runs the **developer "head
   unit server"** and our device is a **TCP client → `phone_ip:5277`**, followed by a **TLS client
   handshake using the embedded Google Automotive Link cert**. No AOAP, no libusb, no Bluetooth, no
   Wi-Fi Direct needed for the *developer* path. This aligns with the "wireless-only, drop USB"
   conclusion already in `android-auto-protocol.md`.
2. **Wired testing is free and easy:** `adb forward tcp:5277 tcp:5277` + connect `127.0.0.1:5277`
   gives us a deterministic ground-truth harness on macOS today, identical to DHU. Use it to
   validate our TLS/framing/proto stack before any Vita work (matches the "dev harness first" next
   step in `aa-open-source-headunits.md`).
3. **Wireless is viable on a stock phone, no root** — but prefer both devices on a shared router
   (avoid phone-as-hotspot). This removes the "wireless bootstrap blocker" that
   `android-auto-protocol.md` flagged for *native* wireless; the developer-server mode sidesteps the
   Bluetooth handoff entirely. The Vita still cannot host an AP, but it doesn't need to for this
   mode — it just dials the phone's LAN IP.
4. **Single-connection server is a real constraint:** the harness must not spam-probe 5277. Add
   "connect once, back off on silent failure" from day one (mirror open-headunit's policy). The
   PSVitaAuto spec should state this as a hard requirement.
5. **Version pinning matters:** AA 17.4+ removed the helper/auto-trigger paths, so our only
   reliable trigger is the manual toggle. Document the exact tap-to-enable steps in the spec and
   accept that the user must start the server by hand (or we later spike a root/companion trigger).
6. **Resolution:** request 720p (1280×720) or 480p for the Vita; 1080p is wasteful. The Vita's
   960×544 screen maps to 720p with letterbox/scale. Video config maps to the `VideoResolution`
   proto, already covered in `aa-control-channel`/`aa-media-channels`.

## Remaining unknowns

- **Exact bind address** of the developer server: whether it binds `0.0.0.0` (all interfaces) or a
  specific interface set. Every implementation treats it as LAN-reachable (openauto/open-headunit
  dial `phone_ip:5277` successfully), but Google doesn't document the bind; verify once on a real
  phone.
- **Certificate acceptance on current AA builds:** aasdk-era HUs use `SSL_VERIFY_NONE` + the fixed
  client cert and still work (open-headunit/LIVI are active), but whether Google tightened
  server-side cert pinning on AA 17.x is unverified for *our* cert key. Validate early.
- **Protocol version drift:** whether the aasdk-era message set (version 1.6) is still accepted
  verbatim by AA 17.x, or needs a newer version string (open-headunit Kotlin source is the current
  reference to diff against).
- **Root/service-start path:** whether a documented `am startservice`/`cmd` invocation exists for
  the gearhead projection service (for a no-touch auto-start); not found in public docs — needs a
  rooted-phone spike to determine if it's even possible.
- **Phone-as-hotspot routing bug** — whether a Vita→phone-hotspot topology can work at all (the
  open-headunit note is about manual port-5277 connect failing on 10.x.x.x); needs a spike if we
  want the Vita and phone to connect with no third AP.
- **What the phone screen shows while projecting** in developer-server mode (blank vs confirmation)
  — minor, but affects the user-visible flow we document.

## Recommended next steps

1. **macOS harness spike (wired):** on a real Android 13/14/15 phone, enable AA developer mode,
   Start head unit server, `adb forward tcp:5277 tcp:5277`, then connect a minimal TLS-client
   (mbedTLS/OpenSSL) to `127.0.0.1:5277` with the aasdk cert and log the handshake + VersionResponse.
   This validates §4 and the TLS path before any Vita code.
2. **Wireless spike:** put phone and Mac on one router, read the phone IP, connect the same harness
   to `phone_ip:5277` (no adb). Confirm the server is LAN-reachable and that the cert is accepted
   without USB. Test the phone-as-hotspot case separately to confirm/deny the routing bug.
3. **Capture ground truth:** Wireshark/tcpdump a full session (handshake → channels → video) to
   confirm the modern AA 17.x wire behavior vs aasdk-era assumptions; diff against open-headunit's
   Kotlin for version strings.
4. **Write the `aa-head-unit-server` + `aa-wireless-setup` feature spec** encoding: phone = server
   (5277), HU = TLS client with fixed cert, single-connection discipline, manual toggle steps, and
   the resolution request (720p/480p).
5. **Later spike (root/auto-start):** determine if a rooted phone can auto-start the server via
   `am`/`cmd`, to remove the manual toggle step for end users.
