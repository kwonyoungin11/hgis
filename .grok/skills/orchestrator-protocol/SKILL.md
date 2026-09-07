---
name: orchestrator-protocol
description: >
  Always-on 3-step orchestrator for every user request, every prompt, every task.
  Forces context7 when library docs are needed, STA on complexity 3+, prompt
  rewrite, parallel expert agents, and matching skills. Use on any message,
  question, bug, feature, review, or when the user runs /orchestrator-protocol.
---

# Orchestrator protocol

Run on **every** user request. Do not wait to be asked.

## 1. Tools

- `context7` — current library/API docs (skip for greetings / no-library asks)
- Complexity 1–2: parent CoT only. Do **not** call `sequentialthinking` MCP.
- Complexity 3–5: `Task` `sequential-thinking-agent` (STA). MCP thinking is optional inside STA.

## 2. Rewrite

Internally fill: Role, Context, Objective, Instructions, Constraints,
Output Format, Tone & Style, Evaluation Criteria, Edge Cases.

## 3. Graph engineering

Pick one: QUICK (solo) / gis / ship / debug / architecture / verify.
FEATURE must `spawn_subagent` (or `/workflow ka-ship`) this turn:
scouts → implementer → reviewer → tester.
Do not narrate a launch. Editing app+core without a spawn is blocked.
Load `ka-graph`, `ka-experts`, `ka-hgis`. Show only the merged result.
Details: `.grok/rules/50-graph-loop.md`.

## 4. Loop engineering

After **this turn** writes `src/` / tests / QSS: build + relevant tests this turn. Quote exit codes.
Fail → fix → re-run. No 완료 without this-turn output.
Docs/hooks/rules-only: skip cmake.
Leftover dirty `src/` from another session does not require cmake on a research turn.

Keep ka-hgis invariants and the intent gate. Do not commit unless asked.
