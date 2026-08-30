---
name: unlazy
description: Run the Grok-native unlazy gates ledger. Use when /unlazy, gates, or "do not stop until it is done".
user-invocable: true
argument-hint: "[solo|tree N] <task>"
---

# /unlazy

Read `.grok/skills/unlazy/SKILL.md`.

1. Write `.unlazy/ka-hgis/GATES.md` from the leaf template before implementing.
2. Write this Grok session id as one line to `.unlazy/ka-hgis/session` so Stop binds among leftover pipelines.
3. Parse with `node .grok/skills/unlazy/scripts/gate-check.mjs --status` (no execution).
4. Approve understood `CHECK:` lines, then `--approve`.
5. Grok Stop already blocks while gates are unmet. Do not run `install-hooks.mjs`.
6. C++ still needs `/ka-hgis-verify`.
