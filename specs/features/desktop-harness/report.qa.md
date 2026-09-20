# QA Report: desktop-harness

Date: 2026-09-16
Spec revision: approved, 2026-09-16
Re-verification: 2026-09-16 (AC4 fix for fps verification)

## Acceptance criteria

| # | Criterion | Result | Evidence |
|---|---|---|---|
| AC1 | `cmake --build build-host` produces the `aa_harness` executable | PASS | Ran `rm -rf build-host && cmake -S . -B build-host && cmake --build build-host` (exit 0). `build-host/aa_harness` exists (1,335,784 bytes, `-rwxr-xr-x`). Target declared in `CMakeLists.txt:73` (`add_executable(aa_harness src/host/harness.c)`). |
| AC2 | `aa_harness --mock --output` completes a full handshake + media flow and writes mock H.264 bytes | PASS | `./build-host/aa_harness --mock --output /tmp/qa_out.h264` → `mock session: OK, video frames: 1, advertised resolution: 2`, exit 0. `/tmp/qa_out.h264` = 7 bytes `00 00 00 01 65 88 84` (Annex-B start code + NAL header), non-empty. Logic at `src/host/harness.c:88-90`; mock media frame at `src/host/mock_phone.c:292-296`. |
| AC3 | `aa_harness --help` prints supported options (host, port, output, mock, resolution, fps) | PASS | `./build-host/aa_harness --help` (exit 0) lists `--host`, `--port`, `--output`, `--mock`, `--resolution`, `--fps`, `--help`. Usage text at `src/host/harness.c:26-35`. |
| AC4 | Advertised video config reflects requested resolution/fps (verified via unit check in mock test) | PASS | `tests/test_control.c:75-98` (`test_video_config`) calls `aa_control_set_video_config(hu, AA_VIDEO_480P, AA_FPS_60)` and now asserts **both** `r.video_resolution == AA_VIDEO_480P` (line 92) **and** `r.video_fps == AA_FPS_60` (line 93). `mock_phone_result` gained a `video_fps` field (`src/host/mock_phone.h:17`), populated in `mock_phone.c:222` from `video_configs[0].video_fps`. Enum values `AA_VIDEO_480P = 1`, `AA_FPS_60 = 2` (`src/core/control.h:29-30`); `./build-host/tests/test_control` → `19 checks, 0 failures`. |
| AC5 | `ctest` runs a mock-based harness smoke test and passes | PASS | `ctest --test-dir build-host --output-on-failure` → `100% tests passed, 0 tests failed out of 8`, incl. `8/8 harness_mock_test ... Passed`. Test registered at `tests/CMakeLists.txt:38`. |
| AC6 | README documents head unit server enablement (wired `adb forward` + wireless) and connect commands | PASS | `README.md:104-132` "Real phone validation (desktop harness)": dev-mode + head unit server enablement, wired `adb forward tcp:5277 tcp:5277`, wireless same-LAN connect, and connect commands (`--host 127.0.0.1`, `--host <phone-ip>`). |

## Bugs found

None. BUG-1 (fps not unit-verified) was resolved by Dev: `mock_phone_result`
gained a `video_fps` field and `test_video_config` now asserts
`r.video_fps == AA_FPS_60`. Confirmed via `./build-host/tests/test_control`
(19 checks, 0 failures).

## Non-functional checks

- Host build: PASS — `export PATH="/opt/homebrew/bin:$PATH" && cmake --build build-host` (exit 0)
- Tests: PASS — `./build-host/tests/test_control` → `19 checks, 0 failures`; `ctest --test-dir build-host --output-on-failure` → 8/8 passed (100%)
- Vita build regression: PASS — `export VITASDK=/opt/homebrew/vitasdk && make` (exit 0) produces `build/toolchain-setup.vpk`
- Lint/style: NOT CONFIGURED

## Verdict

PASS
