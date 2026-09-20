# Spec: toolchain-setup

Status: approved
Created: 2026-09-15
Last updated: 2026-09-15
Approved: 2026-09-15 (user)

## Overview

Establish the PS Vita homebrew toolchain and a buildable hello-world skeleton
that deploys to a real device. This is the foundation every later feature
relies on: a reproducible build, a logging path, and a deploy/test loop.

## Goals

- Install the VitaSDK toolchain (via vdpm) on macOS ARM64.
- Create a CMake project skeleton that cross-compiles a Vita homebrew.
- Build a minimal app (vita2d "hello world" frame) into an installable VPK.
- Wire up libdebugnet UDP logging from device to host.
- Document the build and deploy-to-Vita flow.

## Non-goals

- Any Android Auto protocol, networking, video, or input code.
- Any UI beyond a trivial rendered test frame.
- CI/CD automation (may come later).

## Requirements

### Functional

- F1. A documented build command produces a `.vpk` from a clean checkout.
- F2. The app boots on a real Vita (HENlo/taiHEN) and renders a visible frame.
- F3. A log line emitted from the app is received by a host-side UDP log tool.
- F4. The deploy flow (VitaShell) is documented and works end to end.

### Non-functional

- N1. Build completes on macOS ARM64 with only the documented prerequisites.
- N2. Build output must not depend on host-only (non-Vita) libraries.

## Acceptance criteria

- AC1. Running the documented build command in a clean checkout produces a
  `.vpk` without errors.
- AC2. The VPK installs via VitaShell and, when launched on device, renders a
  non-black frame (verified by screenshot or user report).
- AC3. A test log line emitted from the app appears in the host UDP log receiver.
- AC4. The documented toolchain install steps work on a fresh macOS ARM64
  machine with only the listed brew prerequisites.

## Constraints

- Host: macOS ARM64 (darwin).
- Toolchain: VitaSDK via vdpm; DolceSDK excluded.
- Device must be jailbroken (HENlo ≤3.74 + taiHEN + VitaShell) for real testing.
- No interactive debugger on device; use UDP logs + core dumps.

## Dependencies

- Research: `specs/research/vita-toolchain.md`.

## Risks & unknowns

- vdpm bootstrap and package downloads can be network-flaky; may need a pinned
  toolchain version.
- Real-device testing requires a jailbroken unit (Vita3K as smoke-test fallback).
- CMake requires `$VITASDK` to be exported for toolchain auto-detection.

## Open questions

- ~~Pin an exact VitaSDK version, or track latest?~~ → Track latest via vdpm.
- ~~Preferred deploy method?~~ → FTP via VitaShell (default); USB documented as fallback.
