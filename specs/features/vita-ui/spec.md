# Spec: vita-ui

Status: approved
Created: 2026-09-16
Last updated: 2026-09-16
Approved: 2026-09-16 (user)

## Overview

Give the Vita app a configurable connection target and a minimal session UI
(connection status, host, frame count).

## Goals

- Read the phone host/port from `ux0:data/psvitaauto.cfg` (`ip:port`), falling
  back to a default.
- Show connection status and frame count on screen.

## Non-goals

- Full settings UI / on-screen keyboard.
- On-device validation.

## Requirements

- F1. `load_config` reads the host/port from the config file.
- F2. The render loop shows status + frame count.

## Acceptance criteria

- AC1. Vita build compiles with the config reader + status UI.

## Dependencies

- `vita-video`, `vita-input`.
