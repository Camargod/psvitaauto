---
description: Start the SDD pipeline for a new feature
agent: dev
---

Start the SDD pipeline for the feature: $ARGUMENTS

1. Identify unknowns and delegate them to the `research` persona (task tool).
2. Draft `specs/features/<feature>/spec.md` from
   `specs/.template/spec.template.md`.
3. Add the feature to `specs/STATUS.md` with state `spec`.
4. Present the spec summary and wait for the user's explicit approval before
   any code.
