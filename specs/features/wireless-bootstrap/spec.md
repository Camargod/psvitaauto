# Spec: wireless-bootstrap

Status: approved
Created: 2026-09-16
Last updated: 2026-09-16
Approved: 2026-09-16 (user)

## Overview

Establish how the PS Vita connects to the phone wirelessly and how the phone's
Android Auto server is started. The protocol core is topology-agnostic (TCP
client → phone:5277) and already validated over Wi-Fi; this feature settles the
topology and adds phone discovery.

## Goals

- Lock the topology: Vita as a Wi-Fi client (shared router or phone hotspot).
- Add a phone-discovery helper that probes the LAN for a head unit server on
  port 5277 (mirroring open-headunit's approach).
- Document the trigger workflow (manual "Start head unit server" toggle).

## Non-goals

- Auto-triggering the server (broken on AA 17.4+; needs a rooted phone).
- Hosting a network from the Vita (no AP mode).

## Decisions

- **Topology**: shared router (both on the same Wi-Fi) as primary; phone
  hotspot as secondary. The Vita connects to `phone-ip:5277`.
- **Trigger**: manual toggle for v1; auto-trigger deferred.

## Requirements

- F1. A discovery helper finds a head unit server on the LAN by probing 5277.
- F2. The harness supports `--discover` (find the phone, then connect).
- F3. The wireless + trigger workflow is documented.

## Acceptance criteria

- AC1. `aa_harness --discover` finds the phone's head unit server on the LAN
  and connects.
- AC2. The discovery helper is unit-testable (mock a listening socket).
- AC3. README documents the wireless + toggle workflow.

## Constraints

- Discovery probes a configured subnet range on port 5277 (TCP connect probe).

## Dependencies

- `desktop-harness`, `aa-proto-v2`.
- Research: `specs/research/aa-wireless-bootstrap.md`.

## Open questions

- Phone hotspot gateway address (192.168.43.1) vs the reported 10.x.x.x routing
  bug — to confirm in the spike.
