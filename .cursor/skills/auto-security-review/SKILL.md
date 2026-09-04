---
name: auto-security-review
description: After this-turn product source edits, automatically launch the security-review subagent once without asking which review to run. Use when src/, tests/, CMakeLists.txt, or *.qss were written, after implementing or fixing C++/Qt/QGIS code, or when a stop-hook follow-up says AUTO security review.
---

# Auto security review

Do not ask the user to pick Bugbot vs Security. This project defaults to **Security Review**.

## When it applies

Apply automatically if **this turn** wrote any of:

- `src/`
- `tests/`
- `CMakeLists.txt`
- `*.qss`

Skip when the turn only touched docs, `.cursor/`, `.grok/`, hooks, skills, or was research-only.

## What to do

Launch exactly one `security-review` subagent:

- `run_in_background: false`
- `description: "Security Review"`
- `subagent_type: "security-review"`

Prompt shape (nothing else):

```text
Full Repository Path: D:/hgis
Diff: uncommitted changes
Custom Instructions: Review this-turn product edits only. Do not fix findings.
```

If the stop hook already sent this follow-up, run it once and stop. Do not launch a second review.

## After it finishes

- Empty diff: one sentence, no issues to review.
- No findings: `Security review found no issues`.
- Findings: markdown table `Severity | Location | Finding`, severity high first, location as `file:line`.

Do not fix findings unless the user asks.
