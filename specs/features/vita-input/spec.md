# Spec: vita-input

Status: approved
Created: 2026-09-16
Last updated: 2026-09-16
Approved: 2026-09-16 (user)

## Overview

Forward the PS Vita's touch and button input to the phone as Android Auto
`InputReport` messages (touch + key events) on the input channel.

## Goals

- Encode touch/key events into `InputReport` (host-testable).
- Capture SceTouch/SceCtrl on the Vita and forward them (scaled to the video
  frame space).

## Non-goals

- Rotary/touchpad/absolute input (Vita has none).
- On-device input validation (needs the device).

## Requirements

- F1. `aa_control_send_touch` / `aa_control_send_key` encode + send `InputReport`.
- F2. The Vita app maps SceTouch/SceCtrl → input events (coords scaled
  1920×1088 → 1280×720).

## Acceptance criteria

- AC1. Input encoding round-trips (host unit test).
- AC2. Vita app builds with the input capture wired in.

## Dependencies

- `aa-proto-v2`, `vita-video`.

## Open questions

- Exact keycode mapping for Vita buttons (cross=enter, circle=back, etc.) — a
  first guess is wired; refine on-device.
