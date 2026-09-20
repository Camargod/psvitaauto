# Plan: wireless-bootstrap

Status: approved
Spec: ../wireless-bootstrap/spec.md (approved)
Created: 2026-09-16

## Architecture

A discovery helper probes the local /24 subnet on port 5277 to find the phone's
head unit server; the harness gains a `--discover` mode. Topology stays
Vita-as-client (no hosting).

## Components

1. **`discover.c`** — `aa_discover_probe` (single-IP TCP probe) +
   `aa_discover_head_unit` (local subnet scan).
2. **Harness** — `--discover` flag.
3. **Docs** — README wireless + toggle workflow.

## File layout

```
src/core/discover.h discover.c
tests/test_discover.c
```

## AC mapping

| AC | Satisfied by |
|---|---|
| AC1 | harness `--discover` |
| AC2 | test_discover (loopback listener probe) |
| AC3 | README |

## Build & test strategy

- Host: `cmake -S . -B build-host && cmake --build build-host`.
- Test: `ctest --test-dir build-host --output-on-failure`.
