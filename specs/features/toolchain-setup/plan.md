# Plan: toolchain-setup

Status: approved
Spec: ../toolchain-setup/spec.md (approved)
Created: 2026-09-15

## Architecture

A single CMake project cross-compiled with the VitaSDK toolchain. CMake
auto-detects the toolchain from the `$VITASDK` environment variable via the
toolchain's own `vita.cmake`. The VitaSDK provides `vita_create_vpk()` to
package the built ELF into a `.vpk`. A thin `Makefile` wraps the cmake
invocation so the build command is one step for dev and QA.

## Components

1. **Toolchain + deps (docs)** — README documenting `brew install wget cmake`,
   `vdpm` bootstrap, and `vdpm install libvita2d libdebugnet taihen`. No code.
2. **Build skeleton** — `CMakeLists.txt` + `Makefile`. Produces the `.vpk`.
3. **App** — `src/main.c`: init vita2d, render a solid frame + text, loop until
   exit. Includes libdebugnet init and a startup log line.
4. **Deploy docs** — README section: FTP the VPK via VitaShell to `ux0:/data`,
   install, launch.

## Data flow

source → `make` → cmake cross-compile → `.vpk` → (device) FTP → install →
launch → libdebugnet UDP logs → host `nc -u -l 18194`.

## File layout

```
CMakeLists.txt
Makefile
README.md
src/main.c
```

## Interfaces

- `vita2d` lifecycle: `vita2d_init`, `vita2d_start_drawing`, `vita2d_end_drawing`,
  `vita2d_swap_buffers`.
- `libdebugnet`: `debugNetInit`, `debugNetPrintf` (UDP port 18194).
- `vita_create_vpk(<target> <titleid> <name>)` from the VitaSDK toolchain.

## AC mapping

| AC | Satisfied by |
|---|---|
| AC1 | Build skeleton (CMakeLists + Makefile + `vita_create_vpk`) |
| AC2 | App (`src/main.c`) renders a non-black frame |
| AC3 | App (`src/main.c`) libdebugnet startup log + host `nc` receiver |
| AC4 | README toolchain-install steps |

## Build & test strategy

- Build: `make` (wraps `cmake -S . -B build && cmake --build build`). Clean
  build produces `toolchain-setup.vpk`.
- Log receive: `nc -u -l 18194` on host before launching the app.
- Device test: VitaShell FTP → install VPK → launch → confirm rendered frame +
  received log line (screenshot/user report).
- Smoke fallback: run the ELF under Vita3K if no jailbroken device is available.
