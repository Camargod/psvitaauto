# specs/

SDD artifacts for PSVitaAuto. All features flow through the pipeline defined
in `.opencode/skill/sdd/SKILL.md`.

## Layout

- `features/<feature>/` — `spec.md`, `plan.md`, `tasks.md`, `report.qa.md`
- `research/` — research notes by topic
- `.template/` — templates copied per feature (keep in sync with the skill)
- `STATUS.md` — feature state board

## Driving the pipeline

- New feature: run `/feature <name>` in opencode.
- Approve specs explicitly: the harness never writes code before you approve.
- After implementation the `qa` persona runs automatically; you can also ask
  "run QA on <feature>".
