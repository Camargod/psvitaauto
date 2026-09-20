# Research: vita-hardware
Date: 2026-09-15
Question: What are the PS Vita hardware capabilities and limits relevant to building a head unit (video streaming, wifi, audio, input)?

## Findings

### 1. CPU
- 4x ARM Cortex-A9 MPCore (r2p10, MIDR 0x412FC09A), ARMv7-A with VFPv3-D32 + NEON, out-of-order, 32KB/32KB L1 per core, 2 MiB shared L2 (Primelink PL310 controller). Part of the Sony/Toshiba "Kermit" SoC (CXD5315GG).
- Default clock 333 MHz; official game max 444 MHz; range 41-444 MHz. Kernel plugins (LOLIcon/LOLITA500) unlock a ~500 MHz mode (yifanlu measured ~494 MHz real).
- The SoC also contains "Venezia" (8x Toshiba MeP cores @ 266.7 MHz, VLIW media DSP) used via the Codec Engine API for media tasks (incl. AVC decode), plus a legacy MIPS32 4k for PSP emulation.
- Practical budget: with CPU 444-500 MHz, homebrew has roughly 2000s-era desktop CPU perf per core; NEON-optimized crypto (AES-GCM) and codecs are the way to go. Overclock increases battery drain/heat; battery is 2210 mAh.

### 2. Memory
- 512 MiB LPDDR2 SDRAM (Samsung KLM4G1FE3A-F001, 2x256MiB banks) at 0x40000000, + 128 MiB CDRAM ("VRAM", single-data-rate, 2x 512-bit buses) at 0x20000000. Total ~640 MiB in the SoC stack.
- Physical map (FW 3.60, Vita dev wiki): TrustZone/secure region 0x40200000-0x4FFFFFFF (~254 MiB); non-secure kernel+usermode region 0x52000000-0x5FFFFFFF (224 MiB). DevKits only get extra DRAM banks.
- Realistically available to homebrew: userland apps share the 224 MiB non-secure region with the kernel; community practice shows large homebrew (RetroArch, moonlight) comfortably allocates 100-200+ MiB. An exact per-app allocation ceiling is not documented (see unknowns). VRAM (128 MiB CDRAM) is managed via GXM for textures/render targets.

### 3. GPU
- PowerVR SGX543MP4+ (4 shader cores), exposed via SceGxm; high-level wrappers: vita2d (2D) and VitaGL (GL subset). 128 MiB VRAM.
- Clocks: GPU ES4 default 166 MHz, overclockable to 222 MHz (LOLIcon profiles); GPU bus/XBAR 111-166 MHz; bus 166 MHz (max 222).
- Realistic output: 960x544 @ 60 fps 2D/compositing is trivial (vita2d). Moderate 3D via VitaGL is fine, but 1080p-scale framebuffers are wasteful since the panel is 960x544. The streaming use-case (decode -> blit texture) is GPU-light.

### 4. Video decode
- Hardware AVC decoder (SceVideodec / SceAvcdec, SCE_VIDEODEC_TYPE_HW_AVCDEC). No HEVC, VP8/VP9 hardware decode (software decode of those is not viable on this CPU).
- Decoder accepts dimensions in multiples of 16 (min 64); community tests (MakiseKurisu test repo) confirm resolutions beyond 1280x720 are accepted (≥1920-wide entries), and vita-moonlight ships 1080p stream support; however the officially documented media-player ceiling is 720p H.264 (High/Main/Baseline, level ~3.1).
- Decode output can go straight into GPU-visible RGBA8888/RGBA565/YUV420 buffers (vita-moonlight decodes directly into a vita2d GXM texture). Decode+display pipeline proven in production by vita-moonlight (Gamestream client): 720p60 and 1080p streams decode, with 30 fps being the safe baseline and 60 fps typically requiring CPU overclock + Wi-Fi power-save off.
- Practical target: decode 720p30 (or native 960x544@30) with lots of headroom; 720p60 possible with overclock.

### 5. Display
- 5" 960x544 qHD (~220 ppi), OLED (PCH-1000) or LCD (PCH-2000), 60 Hz fixed refresh (sceDisplayGetRefreshRate). No variable refresh. Scaling: GPU blit; decoder output must be scaled to 960x544 for display (e.g., 16:9 1280x720 -> 960x544 crop/scale, as moonlight does).

### 6. Wifi
- Marvell 88W878S-BKB2 "Avastar" WLAN/Bluetooth/FM combo SoC, connected via SDIO. 802.11b/g/n, 2.4 GHz only, 1x1 SISO (single antenna, no MIMO, no 5 GHz). WPA/WPA2.
- Community benchmarks: Vita system speed test typically reports 10-20 Mbps down when close to AP; real transfers ~0.7-1.3 MB/s (5.6-10.4 Mbps); r/vita users report ceilings of ~12-18 Mbps; FTP via VitaShell ~5 Mbps with Wi-Fi power-save disabled (which moonlight users confirm improves stability). Latency on LAN is fine for streaming (moonlight is usable locally), but no precise ms figures published.
- Power-save mode ("Use Wi-Fi in Power Save Mode" setting) throttles the radio and adds latency — must be disabled for streaming.
- The stock WLAN stack is client-mode only (plus PSP-era ad-hoc); no AP/hotspot mode.

### 7. Bluetooth
- Bluetooth 2.1+EDR (same Marvell combo chip). OS-level support: headsets (A2DP, effectively for the Music app; not usable as low-latency game audio), HID keyboards via Settings, DS3/DS4 pairing (PSTV / Remote Play; on handheld via ds4vita-style plugins).
- Homebrew access is limited (no useful public SceBt user API beyond pairing/status); community uses kernel plugins for controllers.
- Conclusion: BT is not a usable audio output for a head unit; treat BT as controller-only, plan audio via the 3.5 mm jack / speakers.

### 8. Audio
- Output: built-in stereo speakers + 3.5 mm jack; codec chip Wolfson WM1803E. SceAudioOut API: MAIN port fixed at 48000 Hz (S16 mono/stereo), plus BGM and VOICE ports; grain size 64-65472 samples (multiple of 64), blocking output with sceAudioOutGetRestSample for buffer-level monitoring.
- Latency: no published end-to-end numbers; typical homebrew double-buffering of 1024-sample grains implies ~40-60 ms app-level latency + DAC path. Moonlight exposes a configurable local-audio buffer setting (users tune it to fix A/V sync), confirming latency is manageable and tunable.
- Simultaneous audio+video: fully supported (moonlight streams 48 kHz stereo PCM alongside H.264).

### 9. Input
- Front touch: capacitive multitouch (up to 6 reports), rear touchpad (up to 4 reports), 960x544 coordinate space with force values; SceTouch with configurable sampling state (typically 60 Hz, timestamps provided).
- Buttons: full PS layout (d-pad, face, L/R, Start/Select, PS, volume/power) + 2 analog sticks (8-bit per axis in SceCtrlData); sceCtrlPeekBufferPositive supports up to 64 buffered samples with timestamps; polling cadence is app-driven (per-frame at 60 Hz is the norm).
- Motion: 3-axis accelerometer (Kionix KXTC9), 3-axis gyro (STMicro 3GA51H), 3-axis compass.
- Precision: touch reports are ~1 px accurate; sticks are 8-bit — sufficient for AA touch/button input forwarding, marginal for fine pointer work.

## Sources (URLs)
- Wikipedia, PlayStation Vita: https://en.wikipedia.org/wiki/PlayStation_Vita
- Copetti, "PlayStation Vita Architecture (Part 1)": https://www.copetti.org/writings/consoles/playstation-vita/
- Vita Development Wiki (via r.jina.ai proxy; site is bot-protected): Kermit https://wiki.henkaku.xyz/vita/Kermit ; Physical Memory https://wiki.henkaku.xyz/vita/Physical_Memory ; Main Processor https://wiki.henkaku.xyz/vita/Main_Processor ; GPU https://wiki.henkaku.xyz/vita/GPU
- vitaSDK headers (github.com/vitasdk/vita-headers): psp2/videodec.h, psp2/audioout.h, psp2/ctrl.h, psp2/touch.h, psp2/display.h, psp2/power.h
- vitaSDK docs: https://docs.vitasdk.org/ (SceNet socket API: group__SceNetUser.html)
- Teardown / chip IDs: evertiq.com "PlayStation Vita teardown" https://evertiq.com/design/25274 ; psdevwiki "Wireless communications" https://www.psdevwiki.com/vita/Wireless_communications (Marvell 88W878S-BKB2)
- LOLIcon overclock plugin: https://github.com/dots-tb/LOLIcon ; oclockvita: https://github.com/frangarcj/oclockvita ; lolita500: https://github.com/AuroraWright/lolita500
- Reddit r/vita / r/VitaPiracy wifi speed threads: https://www.reddit.com/r/vita/comments/1hy38n/ps_vita_wifi_speed/ ; https://www.reddit.com/r/vita/comments/7wr8bk/can_the_vita_not_achieve_download_speeds_higher/ ; https://www.reddit.com/r/VitaPiracy/comments/qc2ej2/slow_ftp_transfer_speed/
- vita-moonlight (proven decode/render/streaming pipeline): https://github.com/xyzz/vita-moonlight (src/video/vita.c, CHANGELOG.md, wiki)
- sceVideodecInitLibrary resolution test: https://github.com/MakiseKurisu/vita-sceVideodecInitLibrary-test
- Vitaki/vitarps5 wifi notes (1x1 n, no MIMO): https://github.com/mauricio-gg/vitaki-vitarps5/blob/main/docs/WIFI_OPTIMIZATION.md

## Implications for PSVitaAuto
- Video: request 1280x720@30 (H.264 baseline/high, level 3.1) or 960x544@30 from the phone; hardware decode into a 960x544 GXM texture is the proven moonlight pipeline. 720p60 only as stretch goal with 500 MHz overclock + wifi power-save off.
- Wifi: expect 5-15 Mbps usable throughput on 2.4 GHz; Android Auto H.264 streams (~2-5 Mbps at 480p/720p30) fit, but keep the radio on max power (disable power-save) and keep the phone close to the Vita. 1x1 2.4 GHz is the bottleneck, not decode.
- Critical architectural gap: stock firmware provides no Wi-Fi AP mode; wireless Android Auto expects the head unit to host the Wi-Fi network. Options to research: phone-as-hotspot + Vita connects as client (TCP connectivity is what the HUP transport needs), or a bridging device, or wired AA via USB (Vita USB device-mode may be OTG-capable per Kermit pinout).
- Audio: 48 kHz S16 stereo PCM (AA's audio format) maps 1:1 onto the SceAudioOut MAIN port; expect ~40-60 ms latency, tune via grain size; A/V sync needs a shared clock (mirror moonlight's approach).
- CPU: budget one core for network/TLS at 444-500 MHz (NEON-accelerated mbedTLS AES-GCM for the wireless-AA TLS handshake), one for audio, spare cores for protobuf/input. Overclock recommendation: ship with CPU 444 MHz default, optional 500 MHz toggle.
- Memory: hundreds of MB are available; decoder frame buffers, TLS buffers, and stream ring buffers fit comfortably. VRAM: only 960x544 RGBA (2 MiB) is needed for the frame texture.
- Input: touch + buttons + sticks forward cleanly via SceCtrl/SceTouch; no rotary encoder or hard keys needed.
- Bluetooth: unusable for audio output; not needed for AA (phone is the BT host for car audio, not the Vita).

## Remaining unknowns
- Exact per-process RAM allocation ceiling for userland apps (no authoritative published number; 224 MiB non-secure region shared with kernel is the hard bound) — measure empirically with sceKernelAllocMemBlock probe.
- End-to-end audio latency in ms (no published measurements) — measure with a loopback/impulse test.
- Wifi latency (RTT/jitter) under load on 2.4 GHz — measure; community reports stability issues under congestion.
- Max decoder throughput at 1080p (test log truncated in this research; moonlight works but 1080p60 is unverified) — verify at 720p30/60 only, we don't need 1080p.
- Whether any homebrew AP-mode wifi is feasible at all (driver/kernel plugin route, e.g., extending the Marvell firmware usage) — unresolved; determines wireless AA architecture.
- Vita USB device/host capabilities for a potential wired-AA fallback.

## Recommended next steps
1. Spike: probe tool on real hardware measuring max memblock allocation, audio loopback latency, wifi RTT/throughput (TCP + UDP) with power-save off, and CPU headroom during 720p30 H.264 decode+render (extend the moonlight-proven pipeline).
2. Research Android Auto head-unit protocol requirements (resolution/bitrate/fps the phone will send, wireless handshake expectations, whether client-mode Wi-Fi can satisfy the transport) before committing to a wifi topology.
3. Investigate Vita AP-mode feasibility (kernel plugin) in parallel; decide wireless-AA topology (phone hotspot vs AP plugin vs middleman vs USB).
4. Validate TLS 1.2 + protobuf processing cost on 444/500 MHz NEON build to confirm CPU budget.
