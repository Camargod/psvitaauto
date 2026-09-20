# Plan: vita-video

Status: approved
Spec: ../vita-video/spec.md (approved)
Created: 2026-09-16

## Architecture

Refactor the build so the `aa-core` library (transport, framing, TLS, protobuf,
control) + nanopb + mbedTLS compile for BOTH host and Vita. The Vita app then
links core and drives the video path: TCP → control handshake → H.264 → SceAvcdec
→ RGBA → vita2d.

## Components

1. **Build refactor** — move mbedTLS + nanopb + `core` out of the host-only
   branch; Vita branch links core + SceAvcdec + vita2d + net stubs.
2. **Net init** — Vita app calls `sceNetInit`/`sceNetCtlInit` before sockets.
3. **`avcdec`** — SceAvcdec wrapper: init, query size, create, decode AU → RGBA,
   delete.
4. **App** — `src/vita/vita_video.c`: connect, `aa_control`, video callback feeds
   AUs to the decoder, vita2d render loop.

## File layout

```
CMakeLists.txt                 # dual build, core for both
src/core/*                     # unchanged (transport uses newlib sockets on Vita)
src/vita/avcdec.h avcdec.c
src/vita/vita_video.c          # replaces the hello-world main
```

## AC mapping

| AC | Satisfied by |
|---|---|
| AC1 | Vita `make` produces the VPK |
| AC2 | app connect + handshake |
| AC3 | avcdec AU→RGBA |
| AC4 | vita2d render loop |

## Build & test strategy

- Host: `cmake -S . -B build-host && cmake --build build-host` + `ctest`.
- Vita: `make` → `build/*.vpk`. On-device validation needs a jailbroken Vita.
