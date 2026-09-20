# Research: aa-wireless-bootstrap

Date: 2026-09-16
Question: how does the phone's AA head unit server get started wirelessly, and what topology works for a PS Vita client?

## Findings

- The phone's "head unit server" (developer mode) listens on **TCP 5277** and is
  **LAN-reachable** — a client on the same Wi-Fi network connects directly to
  the phone's IP (validated in practice: `192.168.1.73:5277`).
- open-headunit discovers the phone by **probing port 5277 on the LAN**
  (`NetworkDiscovery.kt`: scans candidate IPs, checks 5277, hands the socket to
  the session). Its discovery strategies are: LAN/manual, Google Nearby, phone
  hotspot (host), head-unit hotspot (passive).
- open-headunit's "native AA" mode has a full wireless handshake
  (Wi-Fi Direct / hotspot credential handoff) because it acts as the *network
  host*; the Vita cannot host a network (no AP mode), so that path is closed.
- **The trigger is the hard part.** Starting the server requires the Android
  Auto developer "Start head unit server" toggle. AA 17.4+ broke the third-party
  auto-triggers ("Wireless Helper" app); the manual toggle is the reliable path.
  No public adb/intent command to start it was found.
- The two topologies for the Vita:
  1. **Shared router**: phone + Vita on the same Wi-Fi; Vita connects to
     `phone-ip:5277`. (Validated end-to-end.)
  2. **Phone hotspot**: phone shares its hotspot; Vita joins it and connects to
     the hotspot gateway (e.g. `192.168.43.1:5277`). **Confirmed broken**
     (2026-09-16 on-device test): TCP connects but the head unit server never
     replies to `VERSION_REQUEST` over the phone's own hotspot (tethering network
     isolation) — `st` stuck at 1, `rx=0`. A travel router (both join it) is the
     working in-car substitute.

## Sources (URLs)

- open-headunit source: `connection/wifi/NetworkDiscovery.kt`,
  `connection/wifi/modes/WifiLauncherNative.kt`, `connection/wifi/DiscoveryModePolicy.kt`
  (github.com/andreknieriem/open-headunit, cloned locally)
- `specs/research/aa-head-unit-server.md` (earlier research)
- `specs/research/android-auto-protocol.md` (wireless flow overview)

## Implications for PSVitaAuto

- The Vita is a **Wi-Fi client**; no AP mode, no BT. Topology = shared router or
  phone hotspot.
- The protocol core is topology-agnostic (TCP client → phone:5277); the wireless
  path is already validated end-to-end.
- The unresolved piece is the **trigger**: today it is the manual toggle. An
  auto-trigger would need a phone-side helper (broken on AA 17.4+) or a rooted
  phone.

## Remaining unknowns

- Whether a **travel router** topology works reliably in-car (expected: yes, same as
  shared router; unverified on hardware).
- Any 2026 adb/intent to start the server without the toggle (unverified).

## Recommended next steps

1. Spike: connect the Vita and phone through a **travel router** and confirm the
   stream, to lock in the in-car topology.
2. Accept the manual toggle as the v1 trigger; document it. Revisit auto-trigger
   only if a rooted phone is acceptable.
