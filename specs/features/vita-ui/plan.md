# Plan: vita-ui

Status: approved
Spec: ../vita-ui/spec.md (approved)
Created: 2026-09-16

## Architecture

A `load_config` helper reads `ux0:data/psvitaauto.cfg`; the existing vita2d
status text is retained as the minimal UI.

## File layout

```
src/vita/vita_video.c   # + load_config
```

## AC mapping

| AC | Satisfied by |
|---|---|
| AC1 | Vita make |

## Build & test strategy

- Vita: `make` → `psvitaauto.vpk`.
