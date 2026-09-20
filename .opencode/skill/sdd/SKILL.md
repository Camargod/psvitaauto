---
name: sdd
description: Spec-Driven Development pipeline for PSVitaAuto. Use when starting a feature, writing a spec, planning, implementing tasks, running QA verification, or updating the feature status board. Keywords: feature, spec, plan, tasks, qa, gate, status.
---

# SDD Pipeline

Every feature in this repository flows through this pipeline. It is the single
process for all personas (`dev`, `research`, `qa`). Skip nothing — the
templates exist so every step is fillable mechanically, even by weak models.

## Artifacts per feature

Features live in `specs/features/<feature>/`:

| File | Who writes | When |
|---|---|---|
| `spec.md` | dev (with research input) | kickoff |
| `plan.md` | dev | after spec approval |
| `tasks.md` | dev | after spec approval |
| `report.qa.md` | qa | after implementation |
| `notes.md` (optional) | anyone | any time |

Templates: `specs/.template/`. Status board: `specs/STATUS.md` — update it at
every stage transition.

## Stages

### 0. Kickoff — spec draft

1. Identify the feature and its unknowns.
2. Delegate unknowns to the `research` persona; it writes
   `specs/research/<topic>.md` notes.
3. Create `specs/features/<feature>/spec.md` from
   `specs/.template/spec.template.md`. Fill every section. Acceptance criteria
   must be numbered (AC1, AC2, ...), specific and testable.
4. Set spec Status: `draft`. Add the feature row to `specs/STATUS.md` with
   state `spec`.
5. Present a short summary to the user and WAIT for explicit approval.
   - Approved: set Status `approved` + date. Proceed to stage 1.
   - Rejected: revise. No code may be written before approval. Ever.

### 1. Plan

1. Write `plan.md` from `specs/.template/plan.template.md`. Fill every
   section. Map each AC to the components/tasks that satisfy it.
2. Write `tasks.md` from `specs/.template/tasks.template.md`. Rules:
   - Tasks are ordered, small (finishable in one session), and atomic.
   - Each task has its own Definition of Done (checklist).
   - If no build system exists, task T1 creates it.
3. Update `specs/STATUS.md` -> state `plan`. Present the plan summary to the
   user and wait for approval (explicit is preferred).

### 2. Implement

1. Work strictly in task order; one task at a time.
2. For each task: implement, then prove its DoD (build, run, test), then mark
   it `[x]` in `tasks.md`.
3. On deviation from the spec: stop, update the spec, get re-approval,
   continue.
4. When all tasks are done: update `specs/STATUS.md` -> state `qa`, and invoke
   the `qa` persona for this feature.

### 3. QA

1. `qa` verifies every acceptance criterion with evidence and writes
   `report.qa.md`.
2. Verdict FAIL -> dev fixes each bug, then QA re-runs. Repeat until PASS.
3. Verdict PASS -> stage 4.

### 4. Done

- Update `specs/STATUS.md` -> state `done`.

## Gates (never skippable)

- G1: Spec approved (explicit user approval) before any code.
- G2: Plan exists before implementation.
- G3: Every task has a proven DoD before QA.
- G4: QA verdict PASS before `done`.

## Cross-cutting rules

- The spec is the single source of truth. Code that contradicts it is a bug.
- One feature active at a time on the status board unless the user says
  otherwise.
- Artifacts in English. Dates as YYYY-MM-DD.
- Update `specs/STATUS.md` in the same change as the artifact it reflects.
