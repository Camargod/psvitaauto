# Research: vita-toolchain
Date: 2026-09-15
Question: What toolchain, language, and libraries do we need to develop PS Vita homebrew for this project?

## Findings

### 1. Toolchain: VitaSDK is the standard; DolceSDK is dead

- **VitaSDK (vitasdk.org)** is the current, actively maintained standard toolchain. It ships as
  versioned, signed releases via package channels managed by `vdpm` (the package manager).
  The current supported series is **2026.08** (built on **Newlib 4.1**); a `nightly` channel
  also exists. All downloads are signature-verified (Ed25519 channel manifest).
- **DolceSDK is dead.** The original GitHub org no longer exists; only abandoned forks remain
  (e.g. `DolceSDK2/*`, `Team-CBPS/cbps-sdk`). It must not be used.
- **macOS ARM64 is natively supported.** Every release publishes bootstrap archives for
  `arm64-apple-darwin` (confirmed in autobuilds releases, e.g. `vitasdk-bootstrap-arm64-apple-darwin.tar.bz2`
  built 2026-09-14), and `vdpm` component releases cover macOS arm64. No Rosetta/WSL needed.
- **Prerequisites on macOS:** Xcode CLT (git/make/python) + `brew install wget cmake`.
- **Install:** `export VITASDK=/usr/local/vitasdk; export PATH=$VITASDK/bin:$PATH`, then
  `git clone https://github.com/vitasdk/vdpm && cd vdpm && ./bootstrap-vitasdk.sh`.
  Libraries are installed with `vdpm install <pkg>` (dependencies resolve automatically).
  Updates: `vdpm upgrade` (package fixes only) and `vdpm refresh <series>` (toolchain move).
- **Docker alternative** (useful for CI): `vitasdk/vitasdk:2026.08` — multi-arch manifest
  covering `linux/amd64` and `linux/arm64`, with `$VITASDK` pre-set.
- **Running homebrew on device:** the Vita must be jailbroken. Options: HENkaku (FW 3.60),
  h-encore (3.65–3.68), Trinity (3.69–3.70), or **HENlo** (TheOfficialFloW, WebKit+Kernel
  exploit chain for *all* firmwares incl. 3.74 — the modern standard). The exploit loads
  **taiHEN**, the CFW framework that disables code signature checks and provides the
  plugin/hook substrate. taiHEN ≠ the hack itself; it is the substrate underneath
  HENkaku/h-encore/HENlo, configured via `ux0:tai/config.txt`.

### 2. Language: C and C++ via GCC

- Cross compiler is **GCC-based**: `arm-vita-eabi-gcc` / `arm-vita-eabi-g++`, targeting the
  Vita's ARM Cortex-A9 (ARMv7) with the VFP hard-float ABI by default; a `vita_softfp`
  ABI profile also exists (with shim libraries for the 23 stub symbols that disagree on
  float argument marshalling). Exact GCC version of the 2026.08 series not yet verified
  locally (see Remaining unknowns).
- C: C11/C17 supported. C++: C++11/14/17 supported; official samples use `-std=c++11`
  (e.g. `samples/hello_world/CMakeLists.txt`). C++20 features are partially available on
  modern GCC.
- **The C library is newlib** (not Sony's SceLibc). The toolchain wires newlib's allocator
  into Sony's SceLibc hooks (`user_malloc`, `user_new`, etc. — see vita-elf-create docs), so
  `malloc`/`new` work normally.
- Typical build flags: `-Wall`, `-O2` (set per project); the toolchain file handles the rest
  (no manual `-march`/`-mfloat-abi` needed). Heap size is configurable via
  `sceLibcHeapSize` module variable if needed.

### 3. Build system: CMake (standard)

- CMake is the official build system. `$VITASDK/share/vita.toolchain.cmake` defines the
  toolchain; `include("${VITASDK}/share/vita.cmake")` provides the Vita macros:
  - `vita_create_self(<name>.self <target>)` — ELF → VELF → FSELF (`eboot.bin`)
  - `vita_create_vpk(<name>.vpk <TITLEID> <name>.self ...)` — packages the VPK with
    `NAME`, `VERSION`, and `FILE src dst` entries (icons, LiveArea template.xml).
- Typical project skeleton (from `vitasdk/samples/hello_world`): `CMakeLists.txt` with
  `VITA_APP_NAME`, `VITA_TITLEID` (exactly 9 chars, `XXXXYYYYY`), `VITA_VERSION`,
  `sce_sys/icon0.png`, `sce_sys/livearea/contents/{bg,startup}.png`,
  `sce_sys/livearea/contents/template.xml`, `src/main.c`, linked against the needed
  `*_stub` libraries (e.g. `SceDisplay_stub`).
- `vita-makepkg` builds SDK packages from `VITABUILD` files (in `vitasdk/packages`);
  `vdpm` installs prebuilt ones from the signed channel. Writing a custom `VITABUILD`
  (autotools/CMake/patch flavors) is documented.

### 4. Key libraries (all confirmed present in vitasdk/packages unless noted)

- **libvita2d** — 2D GPU-accelerated graphics over sceGxm (primitives, textures, fonts,
  framebuffer ops). The standard for 2D UI rendering.
- **vitaGL** — OpenGL (1.x/2.x subset) wrapper translating GL to sceGxm with hardware
  acceleration. **Requires `libshacccg.suprx` extracted on the device** (runtime GLSL
  shader compiler; extraction guide: samilops2 vita-troubleshooting-guide). Many big ports
  (Flycast, DaedalusX64, vitaQuake family) use it.
- **SDL2** — full port plus `sdl2_image`, `sdl2_ttf`, `sdl2_mixer`, `sdl2_net`, `sdl2_gfx`
  (and legacy `sdl`/SDL 1.2 variants). Good abstraction if we want portable host-side
  testing of app logic.
- **psp2 API stubs** (Sony firmware APIs, in `vita-headers/include/psp2/`) — the "system
  libraries": `sceGxm` (GPU), `sceDisplay`, `sceNet`/`sceNetCtl` (sockets, WiFi),
  `sceCtrl` (buttons), `sceTouch` (touch), `sceMotion` (accelerometer/gyro), `sceAudioOut`
  (PCM playback), `sceAudiodec` (hardware audio decode), **`sceVideodec`** (hardware video
  decode — H.264/AVC), `scePower`, `sceKernel` (threads), plus dialog/appmgr modules.
  All confirmed present in the headers.
- **TLS/HTTP:** `curl` (and `curl-mbedtls`), `mbedtls`, `openssl` — all packaged. mbedTLS
  is the lightweight choice for the AA TLS link.
- **Protobuf: NO Vita port exists** in vitasdk/packages (checked full package list) and no
  community Vita port found. **nanopb** (pure C protobuf implementation, portable,
  generator-based) is the practical option and can be vendored directly; `protobuf-c` is
  the alternative. Full C++ protobuf would have to be cross-built ourselves.
- Other useful packaged libs: `zlib`, `libpng`, `libjpeg-turbo`, `jansson`/`jsoncpp` (JSON),
  `fmt`, `flatbuffers`, `imgui`/`imgui-vita2d`, `ffmpeg` (heavy — probably overkill),
  `taihen` (taiHEN user/kernel stubs), `libftpvita`.

### 5. Debugging / tooling

- **VitaShell** (TheOfficialFloW) — the standard on-device tool: file manager, package
  installer, built-in FTP and USB (mass storage) transfer. VPKs are copied to `ux0:data`
  via FTP and installed from VitaShell.
- **vita-parse-core** (xyzz) — parses Vita core dumps (crash dumps land in `ux0:data`) into
  a readable stack trace with symbolization. This is the primary crash-debugging path;
  vitasdk.org states debugging support is minimal (no GDB debugger).
- **libdebugnet** (psxdev) — UDP-based remote `printf`/logging for homebrew; pairs with a
  debugnet host tool on the PC. Essential for runtime logging.
- **Vita3K** — experimental emulator (Win/Linux/macOS/Android); runs most homebrew. Useful
  for fast iteration, but network and hardware-decoder emulation fidelity is limited
  (cannot validate real WiFi/TLS/hw-decode paths — hardware testing still required).

### 6. Distribution

- VPK is a zip containing `eboot.bin` (FSELF: unsigned-safe fake-signed SELF), `param.sfo`
  (generated by `vita-mksfoex`, title ID + metadata), and `sce_sys/` assets. Built by the
  `vita_create_vpk` CMake macro or manually with `vita-pack-vpk`.
- Install flow: build VPK → FTP/USB it to `ux0:data` with VitaShell → install as a LiveArea
  bubble. Public distribution happens via VitaDB (`vitadb.rinnegatamante.eu`).

## Sources (URLs)

- https://vitasdk.org/ (install, channels 2026.08, debugging, porting)
- https://vitasdk.org/migration (release model, macOS arm64 bootstrap archives)
- https://github.com/vitasdk/vdpm (vdpm package manager; macOS arm64/x86_64 bundles)
- https://github.com/vitasdk/vita-toolchain (ELF/VELF/SELF/FSELF tools, SceLibc hooks)
- https://github.com/vitasdk/buildscripts (GCC/binutils/newlib build, softfp ABI shims)
- https://github.com/vitasdk/autobuilds (published `arm64-apple-darwin` toolchains)
- https://github.com/vitasdk/packages (full library catalog; curl, mbedtls, libvita2d, SDL2, vitaGL, taihen, libdebugnet…)
- https://github.com/vitasdk/vita-headers (psp2 API headers: sceVideodec, sceAudiodec, sceNet, sceCtrl, sceTouch, sceGxm…)
- https://github.com/vitasdk/samples/blob/master/hello_world/CMakeLists.txt (project skeleton)
- https://github.com/vitasdk/docker (Docker images)
- https://github.com/Rinnegatamante/vitaGL (OpenGL wrapper; libshacccg.suprx prerequisite)
- https://github.com/yifanlu/taiHEN (CFW framework, config.txt, plugin API)
- https://github.com/TheOfficialFloW/VitaShell (file transfer / install tooling)
- https://github.com/TheOfficialFloW/HENlo (jailbreak for all firmwares)
- https://github.com/xyzz/vita-parse-core (core dump parsing)
- https://github.com/psxdev/debugnet (UDP logging library)
- https://github.com/Vita3K/Vita3K (emulator)
- https://github.com/nanopb/nanopb (protobuf option; no Vita port exists)

## Implications for PSVitaAuto

1. **Language + toolchain decision is clear:** C++17 with VitaSDK 2026.08 on macOS ARM64,
   CMake as build system, vdpm for libraries. No forks or dead SDKs.
2. **TLS:** use the packaged `mbedtls` (via `curl-mbedtls` only if needed); VitaSoC WiFi
   is 2.4GHz b/g/n, which bounds wireless AA throughput/latency.
3. **Protobuf:** plan to vendor **nanopb** (pure C, generator on host, runtime on device).
   Do not plan on C++ protobuf. The AA protocol messages (HUP) will need protoc/nanopb
   codegen from the openauto/aasdk proto files — this is a porting task, not an
   install-a-package task.
4. **Video path:** hardware `sceVideodec` (H.264) decode + render via sceGxm (libvita2d
   for 2D UI overlay). VitaGL only if we go GL-heavy and accept the
   `libshacccg.suprx` device prerequisite.
5. **Audio path:** `sceAudioOut` for playback; `sceAudiodec` for compressed audio if the
   phone doesn't send PCM.
6. **Input/network:** sceTouch/sceCtrl/sceMotion for input capture, sceNet/sceNetCtl for
   the TCP socket + WiFi management (AA head unit server mode).
7. **Debugging is minimal** — no interactive debugger: architect around libdebugnet UDP
   logging, core-dump parsing (vita-parse-core), and Vita3K for fast smoke tests.
8. **Device prerequisite:** a hacked Vita (HENlo on ≤3.74) + VitaShell + taiHEN is a hard
   project dependency for any on-device testing.
9. **Distribution:** VPK packaging is fully handled by the CMake macros; LiveArea assets
   are required boilerplate.

## Remaining unknowns

- Exact GCC/binutils versions inside the 2026.08 series (verify after install with
  `arm-vita-eabi-gcc --version`; matters for C++20 feature availability).
- Whether C++ exceptions/RTTI work correctly with the newlib toolchain on Vita (commonly
  avoided in Vita homebrew; needs a spike test).
- Vita3K emulation fidelity for sceNet/sceVideodec — how much of the AA networking/video
  path can be tested without hardware.
- Real-world Vita↔phone WiFi throughput/latency and TLS handshake performance (requires
  on-device benchmarks; affects HUP buffer sizing and video bitrate budget).
- AA HUP specifics: whether nanopb can encode/decode the AA proto schemas without
  modification (message size limits, packed encoding quirks) — follow-up research on
  openauto/aasdk as reference implementation.

## Recommended next steps

1. Install the toolchain on macOS ARM64 (steps in the summary below); build and deploy the
   hello_world sample to verify the full toolchain→VPK→FTP→VitaShell pipeline.
2. Create the PSVitaAuto project skeleton from the hello_world CMake template with a unique
   `VITA_TITLEID`; add libdebugnet logging from day one.
3. Spike: mbedTLS socket (client + server) on the Vita over WiFi to validate TLS and
   measure throughput/latency.
4. Spike: vendor nanopb, generate code from a trivial proto, round-trip on device.
5. Research openauto/aasdk (C++ AA head unit reference) to scope the HUP protocol layer
   and its protobuf schemas before writing the spec for the protocol feature.
6. Acquire/confirm a hacked PS Vita (any FW ≤3.74 with HENlo + taiHEN + VitaShell).

## Install/build steps summary (macOS ARM64)

```
# 1. Prerequisites
brew install wget cmake git make python   # Xcode CLT needed
# 2. Install VitaSDK (current supported series: 2026.08)
export VITASDK=/usr/local/vitasdk
export PATH=$VITASDK/bin:$PATH            # add to ~/.zshrc
git clone https://github.com/vitasdk/vdpm && cd vdpm && ./bootstrap-vitasdk.sh
vdpm status
# 3. Install libraries
vdpm install libvita2d sdl2 mbedtls curl taihen libdebugnet
# 4. Build a project (pass the toolchain file; CMakeLists includes $VITASDK/share/vita.cmake)
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake
cmake --build .                          # produces <name>.self and <name>.vpk
# 5. Deploy: start VitaShell on the Vita, enable FTP, upload VPK to ux0:data, install.
# CI alternative: docker run --rm -v "$PWD:/workspace" vitasdk/vitasdk:2026.08 \
#   sh -c 'cmake -B build -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake && cmake --build build'
```
