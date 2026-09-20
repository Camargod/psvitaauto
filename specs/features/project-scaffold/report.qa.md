# QA Report: project-scaffold

Date: 2026-09-15
Spec revision: approved, 2026-09-15

## Acceptance criteria

| # | Criterion | Result | Evidence |
|---|---|---|---|
| AC1 | Host profile configures and builds with no errors on macOS using the system compiler | PASS | `export PATH="/opt/homebrew/bin:$PATH" && rm -rf build-host && cmake -S . -B build-host && cmake --build build-host` → "The C compiler identification is AppleClang 21.0.0.21000101" (system compiler, no `$VITASDK` in path); builds `libcore.a` and `test_core` with no errors |
| AC2 | `ctest --test-dir build-host` runs and reports the sample test as passed | PASS | `ctest --test-dir build-host --output-on-failure` → "1/1 Test #1: core_test ... Passed 0.22 sec", "100% tests passed out of 1" |
| AC3 | `make` (Vita profile) still produces `build/toolchain-setup.vpk` with no errors | PASS | `export VITASDK=/opt/homebrew/vitasdk && export PATH="/opt/homebrew/bin:$VITASDK/bin:$PATH" && rm -rf build && make` → builds toolchain-setup, velf, self, and vpk with no errors; `ls -la build/toolchain-setup.vpk` → `-rw-r--r-- 1 gabrielcamargo staff 71966 Sep 15 23:21 build/toolchain-setup.vpk` |
| AC4 | Documented layout describes each `src/` subdirectory and its purpose | PASS | `README.md:55-61` "Repository layout" lists `src/core/` (host-testable protocol core, no Vita dependencies), `src/vita/` (Vita app glue), `tests/` (host unit tests), plus `specs/` and `.opencode/` |
| AC5 | Documented conventions state the language (C vs C++) and style per component | PASS | `README.md:63-69` "Conventions" states "Language: C11 everywhere; C++17 allowed later", plus per-component rules: `src/core/` must never include Vita-only headers, tests use in-repo `tests/test.h`, artifacts in English |

## Bugs found

None.

## Non-functional checks

- Host build: PASS — `cmake -S . -B build-host && cmake --build build-host` (AppleClang 21.0.0.21000101, 0 errors)
- Tests: PASS — `ctest --test-dir build-host --output-on-failure` (1/1 passed)
- Vita build: PASS — `make` → `build/toolchain-setup.vpk` (71966 bytes, 0 errors)
- Lint/style: NOT CONFIGURED

## Verdict

PASS
