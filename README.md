# PSVitaAuto

Turn a Sony PS Vita into a wireless Android Auto head unit. This repository is
developed through the SDD pipeline described in `specs/README.md`; this README
covers the toolchain, build, and deploy flow established by the
`toolchain-setup` feature.

## Prerequisites

- macOS (ARM64).
- Xcode Command Line Tools (`xcode-select --install`).
- [Homebrew](https://brew.sh), then: `brew install wget cmake git make python`.
- A jailbroken PS Vita (HENlo on firmware ≤ 3.74 + taiHEN + VitaShell).

## Install the toolchain (VitaSDK)

```sh
export VITASDK=/opt/homebrew/vitasdk     # Apple Silicon; use a user-writable path
export PATH="$VITASDK/bin:$PATH"        # add to ~/.zshrc to persist

git clone https://github.com/vitasdk/vdpm
cd vdpm
./bootstrap-vitasdk.sh

vdpm install libvita2d libdebugnet taihen
```

> Note: on Apple Silicon `/usr/local` is root-owned; install VitaSDK under
> `/opt/homebrew` (user-writable) so the bootstrap does not need sudo.

The current supported toolchain series is 2026.08 (tracked as "latest"). All
downloads are signature-verified by `vdpm`.

## Build

```sh
make
```

This runs `cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake`
followed by `cmake --build build`, producing `build/toolchain-setup.vpk`.
The Makefile defaults `VITASDK` to `/opt/homebrew/vitasdk` unless the env var is set.

## Host build & tests (no Vita toolchain needed)

```sh
cmake -S . -B build-host
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

This builds `src/core` and the test suite on the host using the system
compiler.

## Repository layout

- `src/core/` — host-testable protocol core (no Vita dependencies).
- `src/vita/` — Vita app glue (rendering, input, entrypoint).
- `tests/` — host unit tests (CTest).
- `specs/` — SDD artifacts (features, research, roadmap, status board).
- `.opencode/` — agent personas, skills, commands.

## Conventions

- Language: C11 everywhere; C++17 is allowed later if a component needs it.
- Code in `src/core/` must never include Vita-only headers, so it stays
  host-compilable.
- Tests use the in-repo `tests/test.h` runner (no third-party framework).
- Artifacts (specs, plans, reports) are written in English.

## Deploy (FTP via VitaShell)

1. Connect the Vita to the same Wi-Fi network as your Mac.
2. Open VitaShell on the Vita and press **SELECT** to start the FTP server.
   Note the IP shown (e.g. `192.168.1.20:1337`).
3. Upload the VPK to `ux0:data` with any FTP client:
   `curl -T build/toolchain-setup.vpk ftp://192.168.1.20:1337/ux0:/data/`
4. In VitaShell, browse to `ux0:data`, open `toolchain-setup.vpk`, and install it.
5. Launch the **PSVitaAuto** bubble. A colored screen with
   `PSVitaAuto toolchain OK` should render; press START to exit.

USB (Qcma) transfer is a documented fallback, but FTP is the default flow.

## Logging (libdebugnet)

The app logs over UDP to the host. Before launching, edit `DBGNET_HOST` in
`src/vita/main.c` to your Mac's LAN IP, rebuild, and start a listener on your Mac:

```sh
nc -u -l 18194
```

You should see `PSVitaAuto: toolchain-setup started` and
`PSVitaAuto: entering render loop` when the app runs.

## Debugging

- Logs: libdebugnet (UDP, above).
- Crashes: core dumps land in `ux0:data`; parse them with
  [vita-parse-core](https://github.com/xyzz/vita-parse-core).
- Fast smoke tests without hardware: [Vita3K](https://github.com/Vita3K/Vita3K)
  (network/hardware-decode fidelity is limited).

## Real phone validation (desktop harness)

The host `aa_harness` runs the full head-unit protocol against a real Android
phone's **head unit server** and dumps H.264 to a file.

1. On the phone, enable developer mode: open Android Auto → Settings →
   About → tap "Version and permission info" 10× → allow development settings.
2. Start the head unit server: Android Auto three-dot menu → **Start head unit
   server**.
3. Wired (via adb): `adb forward tcp:5277 tcp:5277`, then connect to
   `127.0.0.1`.
4. Wireless (same LAN): read the phone IP (`adb shell ip addr show wlan0`) and
   connect to it directly — no adb needed.

```sh
cmake --build build-host
# wired:
adb forward tcp:5277 tcp:5277
./build-host/aa_harness --host 127.0.0.1 --output /tmp/session.h264
# wireless (same LAN, explicit IP):
./build-host/aa_harness --host <phone-ip> --resolution 720p --output /tmp/session.h264
# wireless (auto-discover the phone on the LAN):
./build-host/aa_harness --discover --output /tmp/session.h264
```

Notes: the phone is always the TLS server and serves one connection at a time
(connect once, follow through). The mock path needs no phone:

```sh
./build-host/aa_harness --mock --output /tmp/mock.h264
```

The Vita is a Wi-Fi **client** (no AP mode): put the Vita and phone on the same
network (shared router, or the phone's hotspot) and connect to `phone-ip:5277`.
Starting the server is the Android Auto developer toggle (manual for v1 — the
third-party wireless auto-triggers were broken by AA 17.4+).

### Wireless blocker: phone-side authorization

There is no way to remove the phone-side setup entirely. The phone's Android
Auto app only accepts wireless projection from **head units Google recognizes**:
the handshake exchanges certificates signed by Google, and the phone validates
the head unit's identity before accepting a connection. A homebrew Vita has no
(and cannot obtain) a valid Google certificate, so the phone refuses it by
default.

The "debug mode" people enable is actually one of two authorizations:

- **Developer options → "Wireless Android Auto"** (Android 11+): lets the app
  accept any head unit, including unofficial ones.
- **Android Auto → Start head unit server** (developer mode): manually starts
  the projection server.

Both are a **one-time** setup: once enabled (and the Vita paired), the toggle
persists across phone reboots and subsequent connections are automatic — no
per-session activation is needed.

Realistic paths to reduce friction:

| Path | Phone friction | Trade-off |
|---|---|---|
| Developer toggle (Android 11+) | one-time, then automatic | requires enabling dev options once |
| USB-first pairing | zero after first pairing | needs AA-over-USB on the Vita (USB host) first |
| AAWireless dongle | none | extra paid hardware |

The zero-configuration paths are USB-first pairing or a dongle that presents a
Google-recognized identity; a pure homebrew wireless build cannot bypass the
certificate check, so the developer toggle remains the default for v1.

## License

PSVitaAuto is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version. See [LICENSE](LICENSE).

This project links the following third-party components:

| Component | Path | License |
|---|---|---|
| mbedTLS | `third_party/mbedtls` | Apache-2.0 |
| nanopb | `third_party/nanopb` | zlib |
| aap_protobuf (LIVI schemas) | `third_party/aap_protobuf` | GPL-3.0-or-later |

