# PSVitaAuto

Turn a Sony PS Vita into a wireless Android Auto head unit.

## What this project is

An Android Auto head unit implementation running as PS Vita homebrew. The Vita
connects wirelessly to an Android phone, renders the Android Auto UI, forwards
input (touch, buttons, sensors) back to the phone, and plays audio.

Key technical areas:
- Android Auto head unit protocol (Google's proprietary HUP: TCP transport, TLS, protobuf payloads)
- PS Vita homebrew toolchain (vitaSDK / VITASDK, taiHEN, VitaShell ecosystem)
- Wireless networking on the Vita (sockets, wifi configuration, throughput limits)
- Video decode + render pipeline on the Vita (hardware decoders)
- Input forwarding (touch/button encoding over the AA protocol)
- Audio stays on the car's Bluetooth (phone → car stereo); the Vita renders video
  and forwards input only — AA audio channels are negotiated but not output

## How work happens here (SDD)

All features go through the Spec-Driven Development pipeline defined in
`.opencode/skill/sdd/SKILL.md`. Personas: `dev` (implements), `research`
(investigates unknowns), `qa` (verifies against the spec).

Golden rules:
1. Never write code for a feature without an approved spec in
   `specs/features/<feature>/spec.md`.
2. The spec is the single source of truth. Code that diverges from it is a bug.
3. One feature at a time; track everything on `specs/STATUS.md`.
4. Artifacts (specs, plans, reports) are written in English.
5. Model-agnostic: follow templates literally, fill every section, never skip
   a gate. Do not pin models in agent configs.

## Repository layout

- `specs/` — SDD artifacts: features, research notes, templates, status board
- `.opencode/` — agent personas, skills, commands (the harness itself)
- `src/` — source code (created per plan)

## Conventions

- Vita homebrew in C/C++ (vitaSDK); host-side tools per plan.
- Build/test commands are defined per feature in its `plan.md`; run them before QA.
