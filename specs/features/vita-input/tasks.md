# Tasks: vita-input

Spec: ../vita-input/spec.md
Plan: ../vita-input/plan.md
Updated: 2026-09-16

## T1: Input encoding + send

- [x] done

DoD: `aa_control_send_touch`/`send_key` encode `InputReport` and send on the input channel.

## T2: Vita capture

- [x] done

DoD: `vita_video.c` maps SceTouch/SceCtrl → input events (scaled coords).

## T3: Test + build

- [x] done

DoD: `test_input` round-trips; host 10/10 + Vita `make` pass.
