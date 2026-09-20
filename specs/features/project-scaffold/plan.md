# Plan: project-scaffold

Status: approved
Spec: ../project-scaffold/spec.md (approved)
Created: 2026-09-15

## Architecture

One top-level `CMakeLists.txt` with two branches selected by
`CMAKE_CROSSCOMPILING` (the Vita toolchain file sets `CMAKE_SYSTEM_NAME Generic`,
so CMake marks it as cross-compiling). The host branch builds a static `core`
library plus tests; the Vita branch builds the app VPK.

## Components

1. **Layout** — `src/core/` (host-testable protocol code), `src/vita/` (Vita
   app glue), `tests/` (host unit tests).
2. **Dual-profile CMake** — `CMakeLists.txt` branches on `CMAKE_CROSSCOMPILING`.
3. **core placeholder** — `src/core/core.h` / `core.c` with a `core_init()`
   function; no Vita dependencies, so it compiles natively.
4. **Test harness** — `tests/CMakeLists.txt` + `tests/test.h` (minimal
   in-repo assert helper) + `tests/test_core.c` (one sample test). Wired into
   CTest.
5. **Docs** — README section describing the layout and language/style
   conventions per component.

## Data flow

N/A — this is a structural feature; no runtime data flow.

## File layout

```
CMakeLists.txt           # dual-profile (host vs Vita)
Makefile                 # Vita build wrapper (unchanged)
README.md                # + layout & conventions sections
src/
  core/
    core.h               # public header
    core.c               # placeholder impl
  vita/
    main.c               # moved from src/main.c (unchanged logic)
tests/
  CMakeLists.txt
  test.h                 # minimal assert/test runner
  test_core.c            # sample test
```

## Interfaces

- `int core_init(void);` — placeholder returning 0; the sample test asserts it.

## AC mapping

| AC | Satisfied by |
|---|---|
| AC1 | Dual-profile CMake (host branch) + `core` lib |
| AC2 | `tests/` + CTest wiring |
| AC3 | Vita branch of CMake + existing Makefile |
| AC4 | README layout section |
| AC5 | README conventions section |

## Build & test strategy

- Host build: `cmake -S . -B build-host && cmake --build build-host`
  (no `$VITASDK` needed).
- Test: `ctest --test-dir build-host --output-on-failure`.
- Vita build (regression): `make` → `build/toolchain-setup.vpk`.
