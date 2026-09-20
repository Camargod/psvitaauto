# Plan: packaging-release

Status: approved
Spec: ../packaging-release/spec.md (approved)
Created: 2026-09-16

## Architecture

Static `sce_sys/` assets + `vita_create_vpk(... FILE ...)` entries.

## File layout

```
sce_sys/icon0.png
sce_sys/livearea/contents/{bg,startup}.png
sce_sys/livearea/contents/template.xml
```

## AC mapping

| AC | Satisfied by |
|---|---|
| AC1 | Vita make + VPK listing |

## Build & test strategy

- Vita: `make` → `psvitaauto.vpk` (verify contents).
