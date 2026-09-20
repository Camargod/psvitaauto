# Spec: project-scaffold

Status: approved
Created: 2026-09-15
Last updated: 2026-09-15
Approved: 2026-09-15 (user)

## Overview

Define the source tree layout, the dual build (host + Vita), and the test
harness for the whole PSVitaAuto project. This turns the hello-world skeleton
from `toolchain-setup` into a structure that can host the AA protocol core
(testable on macOS without a Vita) and the Vita app glue.

## Goals

- Establish a `src/` layout that separates host-testable protocol code from
  Vita-only code.
- Add a host (native macOS) build profile so protocol code and tests compile
  and run without the Vita toolchain.
- Introduce a minimal unit-test harness (CTest) with at least one passing test.
- Keep the Vita build (from `toolchain-setup`) working as a regression check.
- Document the layout and coding conventions.

## Non-goals

- Any AA protocol / networking / video implementation.
- A CI pipeline (documented conventions only).
- On-device test automation.

## Requirements

### Functional

- F1. A host build target compiles the (empty) `src/core` library natively on
  macOS using the system toolchain.
- F2. A test target runs under CTest and at least one test passes.
- F3. The Vita build still produces `toolchain-setup.vpk` (regression).
- F4. The repository layout and conventions are documented.

### Non-functional

- N1. `src/core` must not depend on any Vita-only header or symbol, so it stays
  host-compilable.
- N2. The test harness adds no third-party dependencies (plain CTest + a small
  in-repo assert helper).

## Acceptance criteria

- AC1. `cmake -S . -B build-host -DCMAKE_BUILD_TYPE=Debug` (host profile)
  configures and builds with no errors on macOS, using the system compiler.
- AC2. `ctest --test-dir build-host` runs and reports the sample test as passed.
- AC3. `make` (Vita profile) still produces `build/toolchain-setup.vpk` with no
  errors.
- AC4. A documented layout (README or `docs/`) describes each `src/`
  subdirectory and its purpose.
- AC5. A documented convention section states the language (C vs C++) and
  style for each component.

## Constraints

- Host build must not require `$VITASDK`; Vita build must not require host
  tools beyond the documented set.
- No third-party test framework dependency.

## Dependencies

- `toolchain-setup` (build skeleton and toolchain docs).
- Research: `specs/research/vita-toolchain.md`.

## Risks & unknowns

- Keeping one `CMakeLists.txt` working for both host and Vita profiles (two
  toolchains in one tree). Mitigation: split profiles by a `VITA_BUILD` option
  or separate top-level CMake presets.

## Open questions

- ~~Language default for `src/core`?~~ → C11 everywhere initially; C++17 allowed
  later per component.
