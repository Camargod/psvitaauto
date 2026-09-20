# Tasks: project-scaffold

Spec: ../project-scaffold/spec.md
Plan: ../project-scaffold/plan.md
Updated: 2026-09-15

Order matters. Mark `[x]` only when the DoD is proven.

## T1: Restructure src/ layout

- [x] done

DoD:

- [x] `src/main.c` moved to `src/vita/main.c` (logic unchanged)
- [x] `src/core/core.h` and `src/core/core.c` created with `core_init()`
- [x] `src/core` has no Vita-only include or symbol

## T2: Dual-profile CMake

- [x] done

DoD:

- [x] `CMakeLists.txt` branches on `CMAKE_CROSSCOMPILING`
- [x] Host: `cmake -S . -B build-host && cmake --build build-host` succeeds, builds `core`
- [x] Vita: `make` still produces `build/toolchain-setup.vpk`

## T3: Test harness

- [x] done

DoD:

- [x] `tests/CMakeLists.txt`, `tests/test.h`, `tests/test_core.c` present
- [x] `ctest --test-dir build-host --output-on-failure` runs and passes
- [x] No third-party test dependency

## T4: Documentation (layout + conventions)

- [x] done

DoD:

- [x] README documents each `src/` subdirectory and its purpose
- [x] README documents language (C11) and style per component
- [x] Host-build and test commands documented
