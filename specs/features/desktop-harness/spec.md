# Spec: desktop-harness

Status: approved
Created: 2026-09-16
Last updated: 2026-09-16
Approved: 2026-09-16 (user)

## Overview

A host (macOS) executable that runs the full Android Auto head-unit flow
against a real phone or an in-process mock peer, dumping decoded H.264 video to
a file. This is the real-phone validation gate for the protocol core built in
Phase 1.

## Goals

- `aa_harness` executable in the host build.
- Connect as a TCP client to a phone's head unit server (default port 5277).
- Run the control handshake + media channels and dump video frames to a file.
- A `--mock` mode that runs the same flow against an in-process mock peer, for
  self-test without a phone.
- Configurable video resolution/fps for the advertised video config.
- Documentation for enabling head unit server mode on the phone.

## Non-goals

- Video rendering/display (Phase 4, Vita).
- Input forwarding (Phase 4 `aa-input`).
- Any Vita code.

## Requirements

### Functional

- F1. `aa_harness` connects to `(host, port)` and runs the head-unit flow.
- F2. Video H.264 frames are written (Annex-B) to an output file.
- F3. `--mock` runs the flow against the mock peer and produces the same video
  output, with no network or phone.
- F4. Video resolution/fps are configurable via CLI and applied to the
  advertised `VideoConfig`.
- F5. Session progress is logged to stdout.

### Non-functional

- N1. Host-only; builds and runs on macOS.
- N2. No phone required for the automated test path.

## Acceptance criteria

- AC1. `cmake --build build-host` produces the `aa_harness` executable.
- AC2. `aa_harness --mock --output /tmp/out.h264` completes a full handshake and
  media flow and writes the mock H.264 bytes to `/tmp/out.h264`.
- AC3. `aa_harness --help` prints the supported options (host, port, output,
  mock, resolution, fps).
- AC4. The video config advertised by the head unit reflects the requested
  resolution/fps (verified via a unit check in the mock test).
- AC5. `ctest` runs a mock-based harness smoke test and it passes.
- AC6. README documents head unit server enablement (wired `adb forward` and
  wireless) and the connect commands.

## Constraints

- Uses the existing `aa_control` / `aa_session` / `aa_msg` stack unchanged.
- The phone is the TLS server; the harness is the TLS client with the AA cert.

## Dependencies

- `aa-control-channel`, `aa-media-channels`.
- Research: `specs/research/aa-head-unit-server.md`.

## Risks & unknowns

- Real-phone interop (cert acceptance, cipher suite, protocol version) is not
  automatable here; it is the manual step this harness enables.
- The mock peer models PLAIN media frames; a real capture would confirm.

## Open questions

- ~~Default video resolution?~~ → 720p default, configurable.
