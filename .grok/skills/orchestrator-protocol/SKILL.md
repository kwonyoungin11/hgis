---
name: orchestrator-protocol
description: >
  Always-on 3-step orchestrator for every user request, every prompt, every task.
  Forces context7 MCP, sequential-thinking MCP, prompt rewrite, parallel expert
  agents, and matching skills. Use on any message, question, bug, feature, review,
  or when the user runs /orchestrator-protocol.
---

# Orchestrator protocol

Run on **every** user request. Do not wait to be asked.

## 1. Tools

Call `search_tool` then `use_tool` for:

- `context7` — current library/API docs
- `sequential-thinking` — decompose the request

## 2. Rewrite

Internally fill: Role, Context, Objective, Instructions, Constraints,
Output Format, Tone & Style, Evaluation Criteria, Edge Cases.

## 3. Graph engineering

Pick one: QUICK (solo) / gis / ship / debug / architecture.
FEATURE must spawn scouts then implementer → reviewer → tester this turn.
Load `ka-graph`, `ka-experts`, `ka-hgis`. Show only the merged result.

## 4. Loop engineering

After code edits: build + relevant tests this turn. Quote exit codes.
Fail → fix → re-run. No 완료 without this-turn output.

Keep ka-hgis invariants and the intent gate. Do not commit unless asked.
