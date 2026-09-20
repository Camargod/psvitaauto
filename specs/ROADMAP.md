# PSVitaAuto Roadmap

Status: draft
Created: 2026-09-15
Based on: `specs/research/` notes (vita-toolchain, vita-hardware, android-auto-protocol, aa-open-source-headunits)

## Decisions locked by research

| Decision | Choice | Rationale |
|---|---|---|
| Language | C11 everywhere (C++17 available if a component needs it) | Clean-room core is C; nanopb/mbedTLS/sce* are all C |
| Toolchain | VitaSDK (via vdpm) + CMake | DolceSDK dead; CMake auto-detects `$VITASDK` |
| Build/host | macOS ARM64 native; no emulator as primary | Vita3K only for smoke tests |
| TLS | mbedTLS | OpenSSL port is heavier; NEON available |
| Protobuf | nanopb (vendored) | No Vita port exists; regenerate from the modern `aap_protobuf` schemas (LIVI) |
| Graphics | libvita2d (sceGxm) | Proven AA-style UI + H.264 texture path (moonlight pipeline) |
| Video decode | SceAvcdec (H.264) | Only hardware codec that matters; 960x544@30 target |
| Audio | **None on Vita** — audio stays on the car's BT (phone → car stereo) | Vita is display + input only; no latency budget needed |
| Logging/debug | libdebugnet UDP logs + vita-parse-core dumps | No interactive debugger exists |
| Strategy | Reimplement, don't port | Reimplement TCP+TLS+framing core in C, using the modern `aap_protobuf` schema (via LIVI/open-headunit) as reference |
| License | GPL-3.0 | Reference schemas are GPL-3.0-or-later; normal for Vita homebrew |

## Hard constraints (from vita-hardware)

- WiFi: 2.4GHz 802.11n 1x1, real-world 5–15 Mbps (ceiling ~18) — enough for AA's 2–5 Mbps H.264
- **No AP/hotspot mode in stock firmware** — biggest architectural risk for wireless bootstrap
- CPU: 4x Cortex-A9 (444MHz official, ~500 via plugin); RAM: ~400MB usable
- Vita BT works at system level (headphones pair fine) but homebrew cannot use it programmatically — irrelevant for audio since the car's BT handles it; however AA wireless normally starts via a BT handoff → needs a phone-side trigger
- Device must be jailbroken (HENlo ≤3.74 + taiHEN + VitaShell)

## Phases (each box = one SDD feature; order matters)

### Phase 0 — Foundation
- [x] `toolchain-setup` — VitaSDK install on macOS, CMake skeleton, hello-world VPK, libdebugnet logging, deploy-to-Vita flow
- [x] `project-scaffold` — src/ layout, build system, test harness strategy

### Phase 1 — Protocol core (`aa-core`, host-testable)
- [x] `aa-transport` — TCP client (port 5277) + AA framing codec (channel ID, FIRST/CONSECUTIVE reassembly)
- [x] `aa-tls` — mbedTLS decoupled record layer (in-band handshake, encrypted record per message) — spike first
- [x] `aa-proto` — nanopb regeneration of schemas; message encode/decode
- [x] `aa-control-channel` — version negotiation, service discovery, ping, channel open/close, focus/auth
- [x] `aa-media-channels` — video/audio/input channel lifecycle
- [x] `aa-proto-v2` — migrate to the modern `aap_protobuf` schemas (aasdk was stale) + real-phone validation

### Phase 2 — Desktop harness (de-risk before Vita)
- [x] `desktop-harness` — run aa-core on macOS against a real phone; **validated: full handshake + H.264 streaming (AA 17.6 / Android 16)**

### Phase 3 — Wireless bootstrap (make-or-break spike)
- [ ] `wireless-bootstrap` — phone-hosted hotspot + client-mode Vita + phone-side trigger (vs Vita AP kernel plugin). Spike first, architecture decision gate after

### Phase 4 — Vita integration
- [x] `vita-video` — H.264 NALs → SceAvcdec → vita2d (builds to `psvitaauto.vpk`; on-device render pending)
- [x] `vita-input` — SceTouch/SceCtrl → TouchEvent/ButtonEvent (encoding + capture; on-device pending)
- [x] `vita-ui` — connection/session UI + config file (builds; on-device pending)

### Phase 5 — Hardening
- [ ] `stability-performance` — overclock, latency tuning, reconnect/error recovery
- [x] `packaging-release` — VPK with LiveArea assets

## Risks

1. **Wireless bootstrap** — no AP mode, no BT handoff; needs spike before committing (Phase 3).
2. ~~Protocol drift~~ — **resolved**: migrated to the modern `aap_protobuf` schemas and validated against AA 17.6.
3. **Cert acceptance** — reverse-engineered TLS cert is a gray area; Google may change acceptance.
4. **Perf budget** — TLS+protobuf+TCP at 444MHz must be measured before Vita work (host is fine; Vita still unmeasured).

## How to proceed

Each feature above starts via `/feature <name>` in opencode, following the SDD pipeline
(`.opencode/skill/sdd/SKILL.md`). Phase 0 first: `/feature toolchain-setup`.
