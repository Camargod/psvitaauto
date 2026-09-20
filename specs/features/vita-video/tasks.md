# Tasks: vita-video

Spec: ../vita-video/spec.md
Plan: ../vita-video/plan.md
Updated: 2026-09-16

## T1: Build refactor (core for host + Vita)

- [x] done

DoD: mbedTLS + nanopb + `core` build for both; Vita `make` links core.

## T2: avcdec wrapper

- [x] done

DoD: `avcdec.c` init/create/decode/delete; AU→picture.

## T3: Vita app (connect + handshake + render)

- [x] done

DoD: `vita_video.c` connects, runs `aa_control`, feeds H.264 to avcdec, renders.

## T4: Build + smoke

- [x] done

DoD: `make` (Vita) produces `psvitaauto.vpk`; host tests (9/9) still pass.
