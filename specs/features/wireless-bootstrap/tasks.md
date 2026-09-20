# Tasks: wireless-bootstrap

Spec: ../wireless-bootstrap/spec.md
Plan: ../wireless-bootstrap/plan.md
Updated: 2026-09-16

## T1: Discovery helper

- [x] done

DoD: `aa_discover_probe` (single-IP TCP probe) + `aa_discover_head_unit` (/24 scan).

## T2: Harness --discover

- [x] done

DoD: `aa_harness --discover` scans the LAN and connects to the found server.

## T3: Test + docs

- [x] done

DoD: `test_discover` (loopback open/closed probe) passes; README documents wireless + toggle.
