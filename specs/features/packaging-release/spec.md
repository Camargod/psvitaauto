# Spec: packaging-release

Status: approved
Created: 2026-09-16
Last updated: 2026-09-16
Approved: 2026-09-16 (user)

## Overview

Package the Vita app as a proper homebrew VPK with LiveArea assets (icon,
background, startup, template).

## Goals

- Add `sce_sys/` LiveArea assets (icon0.png, bg.png, startup.png, template.xml).
- Wire them into `vita_create_vpk`.

## Acceptance criteria

- AC1. `make` (Vita) produces a VPK containing eboot.bin + param.sfo + the
  LiveArea assets.

## Dependencies

- `vita-video`, `vita-input`, `vita-ui`.
