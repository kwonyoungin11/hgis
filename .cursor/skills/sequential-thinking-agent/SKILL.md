---
name: sequential-thinking-agent
description: Routes complexity 3–5 reasoning to the isolated Sequential Thinking Agent (STA). Use for multi-module bugs, unknown-cause defects, architecture, concurrency, or security impact. Do not use for greetings, typos, or single-file edits (score 1–2).
---

# Sequential Thinking Agent (parent router)

Official Cursor subagents: https://cursor.com/docs/subagents

STA is a custom subagent, not a replacement for Context7. Context7 still fetches library docs. Sequential Thinking MCP is optional inside STA only.

## Complexity gate (score once, silently)

| Score | Handle | Examples |
| --- | --- | --- |
| 1–2 | Parent only. No STA. No `sequentialthinking` MCP. | Greeting, typo, one function, one-file edit, simple explain |
| 3–5 | Dispatch `sequential-thinking-agent` this turn | 3+ modules, unknown-cause bug, architecture, concurrency, security impact |

Do not spawn STA on every user message.

## Dispatch

1. If `.grok/.state/sta-session.json` has a recent `agentId` for the same problem, `Task` with `resume` set to that id.
2. Else `Task` `subagent_type: "sequential-thinking-agent"` `model: inherit` `run_in_background: true`.
3. Payload: see [contracts.md](contracts.md). Include `query`, `min_steps` (default 4), `max_steps` (default 12, hard cap 15), `known_facts`, `target_files`.
4. After launch, write `{ "agentId", "taskId" }` to `.grok/.state/sta-session.json` (gitignored).
5. Do not give the user a final answer until STA returns `status: COMPLETED`. Parallel scouts may run while it works.
6. Merge only `finalConclusion`, `rejectedHypotheses`, `actionablePlan`. Drop the scratchpad.

## Heartbeat

Cursor has no child `send_message`. Background output lives under `~/.cursor/subagents/`. Read that or wait for the completion notification.

## Dual mode

STA uses `sequentialthinking` MCP if present, else a Markdown scratchpad. Parent does not need the MCP for score 1–2.

## Verification

| ID | Pass |
| --- | --- |
| TC-01 | False premise is grep-disproved, not invented |
| TC-02 | Three branches, then a tradeoff |
| TC-03 | Easy question still takes ≥ 4 steps |
