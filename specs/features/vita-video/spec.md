# Spec: vita-video

Status: approved
Created: 2026-09-16
Last updated: 2026-09-16
Approved: 2026-09-16 (user)

## Overview

Build the `aa-core` protocol stack for the PS Vita and render the Android Auto
video stream on the device: H.264 NALs → SceAvcdec (hardware decoder) → RGBA →
vita2d texture. This is the first Vita-side feature that turns the validated
protocol into something visible on the device.

## Goals

- Compile the `aa-core` library for the Vita (Vita build includes core).
- A Vita app that connects to the phone (TCP), runs the control handshake, and
  receives the video stream.
- Decode H.264 access units with SceAvcdec into RGBA8888.
- Render decoded frames with vita2d.

## Non-goals

- Input forwarding (follow-up `vita-input`).
- Connection UI beyond a minimal status screen (follow-up `vita-ui`).
- Audio (stays on the car's BT).

## Requirements

- F1. Vita build produces a VPK that links `aa-core` + SceAvcdec + vita2d.
- F2. The app connects to a phone (host:port) and runs `aa_control`.
- F3. The video callback feeds H.264 to SceAvcdec and decodes to RGBA.
- F4. Decoded frames are drawn via vita2d.

## Acceptance criteria

- AC1. `make` (Vita profile) produces a VPK containing the app + core.
- AC2. The app connects and completes the handshake against a mock/phone (logs).
- AC3. A known H.264 sample decodes to a non-black RGBA frame (unit-testable on
  host via a stub, or on-device).
- AC4. The render loop draws the decoded frame (on-device).

## Constraints

- SceAvcdec: `SCE_VIDEODEC_TYPE_HW_AVCDEC`; RGBA8888 output; access-unit input.
- vita2d for rendering (proven moonlight-style decode-to-texture path).

## Dependencies

- `aa-proto-v2`, `wireless-bootstrap`.
- Research: `specs/research/vita-toolchain.md`, `vita-hardware.md`.

## Open questions

- On-device validation requires a jailbroken Vita (Vita3K has limited decoder
  fidelity). The build + a host-side decode test are the automatable checks.
