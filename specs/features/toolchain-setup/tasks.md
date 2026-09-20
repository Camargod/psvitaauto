# Tasks: toolchain-setup

Spec: ../toolchain-setup/spec.md
Plan: ../toolchain-setup/plan.md
Updated: 2026-09-15

Order matters. Mark `[x]` only when the DoD is proven.

## T1: Toolchain install + prerequisites documentation

- [x] done

DoD:

- [x] README documents brew prerequisites, vdpm bootstrap, and `vdpm install libvita2d libdebugnet taihen`
- [x] `$VITASDK` export and PATH setup documented
- [x] Steps verified on the dev machine (`vdpm` runs; libs present)

## T2: Build skeleton (CMake + Makefile)

- [x] done

DoD:

- [x] `CMakeLists.txt` + `Makefile` present
- [x] Clean build (`make`) produces `toolchain-setup.vpk` with no errors
- [x] `vita_create_vpk` packaging works (titleid + name set)

## T3: Hello-world vita2d app

- [ ] done (pending on-device render)

DoD:

- [x] `src/main.c` inits vita2d and renders a non-black frame + text
- [x] Builds cleanly as part of `make`
- [ ] Renders on device (or Vita3K smoke) — non-black frame confirmed

## T4: libdebugnet logging

- [ ] done (pending on-device log)

DoD:

- [x] `src/main.c` inits libdebugnet and emits a startup log line
- [ ] Host `nc -u -l 18194` receives the line from the running app
- [x] Logging documented in README

## T5: Deploy flow (FTP via VitaShell)

- [ ] done (pending on-device install)

DoD:

- [x] README deploy section: FTP VPK to `ux0:/data`, install, launch
- [x] USB documented as fallback
- [ ] VPK installs and launches on device end to end
