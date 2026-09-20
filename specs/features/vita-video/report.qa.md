# QA Report: vita-video

Date: 2026-09-16
Spec revision: approved, 2026-09-16

## Acceptance criteria

| # | Criterion | Result | Evidence |
|---|---|---|---|
| AC1 | Vita `make` produces a VPK with app + core | PASS | `make` (Vita) → `build/psvitaauto.vpk` (473 KB); full stack (mbedTLS + nanopb + core + SceAvcdec + vita2d) cross-compiled |
| AC2 | App connects + completes handshake | NOT VERIFIED | Code compiled; requires a jailbroken Vita on the phone's network (on-device step) |
| AC3 | H.264 sample decodes to RGBA | NOT VERIFIED | `avcdec.c` compiled; decode correctness needs on-device validation |
| AC4 | Render loop draws the frame | NOT VERIFIED | vita2d render loop compiled; needs on-device |

## Bugs found

None (build-level). mbedTLS needed Vita porting fixes: ASM disabled,
`MBEDTLS_NO_PLATFORM_ENTROPY` + custom `sceKernelGetRandomNumber` entropy,
`MBEDTLS_PLATFORM_MS_TIME_ALT` + custom `mbedtls_ms_time`, and
`MBEDTLS_TIMING_C`/`MBEDTLS_NET_C` disabled (unused for TLS-over-custom-transport).

## Non-functional checks

- Vita build: PASS (`psvitaauto.vpk`)
- Host build + tests: PASS (9/9)
- Lint/style: NOT CONFIGURED

## Verdict

PASS (build + compile milestone). On-device validation (AC2-AC4) is the
remaining manual step and requires a jailbroken Vita.
