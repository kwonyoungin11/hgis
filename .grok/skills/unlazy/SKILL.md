---
name: unlazy
description: Make incomplete work visible and make completion testable. Use when writing, verifying, or reporting on multi-step work whose quiet incompleteness would be costly — builds, deep reviews, orchestrated fan-out. Proves outcomes against a gate ledger instead of trusting a confident done report.
---

# Unlazy (Grok)

Make incomplete work visible and make completion testable. Prove outcomes against a ledger instead of relying on a confident done report.

This copy is the Grok-native skill at `.grok/skills/unlazy/`. `<skill-dir>` = `.grok/skills/unlazy`. Requires Node >= 16 on PATH.

Do not create gates for a trivial edit or factual reply. Use this discipline when the cost of quiet incompleteness justifies the ledger.

ka-hgis C++ still needs `/ka-hgis-verify` (cmake / ctest / smoke). Unlazy does not replace that loop.

## Arm the scope first

Before writing gates, create the scope and bind this session to it. An unbound scope fails open, so skipping this step silently disables every gate you are about to write:

```text
node .grok/skills/unlazy/scripts/arm-scope.mjs <scope-id> <grok-session-id>
```

It creates `.unlazy/<scope-id>/`, writes `session` as BOM-free UTF-8 with no trailing whitespace, reads it back, and prints the bound id. Do not arm a scope with `echo`: `echo %ID%> file` in `cmd.exe` leaves a trailing space before the redirect, and PowerShell's `echo`/`>` writes UTF-16 with a BOM. Either one makes the Stop hook's id comparison fail silently, which fails open. If you must do it by hand, use a byte-exact writer:

```text
node -e "require('fs').mkdirSync('.unlazy/<scope-id>',{recursive:true});require('fs').writeFileSync('.unlazy/<scope-id>/session','<grok-session-id>','utf8')"
```

Confirm the binding took before you rely on it. A scope whose `session` file is missing, mis-encoded, or stale does not arm the Stop hook, and the run will end without enforcement.

## Write gates before real work

Use a scoped ledger so a leftover file cannot trap every Grok turn:

```text
.unlazy/ka-hgis/GATES.md
```

Legacy `GATES.md` at the repo root is read by the checker but **does not arm the Grok Stop hook**. A root ledger gives you verification on demand and no enforcement at exit; only a scoped, session-bound ledger blocks a premature stop. Prefer a scoped ledger unless you deliberately want checks without enforcement.

Start from templates/gates-leaf.md. State one observable outcome per gate. Give every runnable gate an indented `CHECK:` and `EXPECT:`; use a manual gate only when no command can decide the outcome.

## Lint the ledger before you trust it

`gate-check.mjs` proves a declared command passed. It cannot tell you the command was the wrong oracle, that an expectation was weakened after the gate went green, or that a checked box has no evidence. Run the static audit, which executes nothing:

```text
node .grok/skills/unlazy/scripts/gates-lint.mjs .unlazy/ka-hgis/GATES.md --strict
```

It fails on duplicate ids, a `CHECK:` with no `EXPECT:`, an `EXPECT:` that appears inside its own `CHECK:` line, a trivially satisfiable `EXPECT:`, a checked box with no `EVIDENCE:`, a boilerplate `ABANDON` reason, an apparent credential in a command, and an oracle edited after that gate was recorded met. It warns on absence gates with no `CONTROL:`, on an `EXPECT:` that reuses a figure from its own title, and on an `ABANDON` whose id was deleted rather than kept. It prints `GATES_LINT_OK` on a clean ledger, so make the lint its own first gate.

**Gate budget.** Cap a leaf at roughly five gates and a branch at roughly eight. Each must name a distinct observable outcome; two gates that fail together are one gate. The ledger costs tokens and wall-clock on every pass, so a leaf that needs fifteen gates is a leaf that should have been split.

## Approve before you execute

Treat `CHECK:` as code. Before executing an inherited ledger, parse it without running anything and read every command and called script:

```text
node .grok/skills/unlazy/scripts/gate-check.mjs --status .unlazy/ka-hgis/GATES.md
```

Approve only commands you wrote or understand, then run them explicitly:

```text
node .grok/skills/unlazy/scripts/gate-check.mjs --approve .unlazy/ka-hgis/GATES.md
```

When an oracle has no existing approval, a normal run prints `CHECK:`, `EXPECT:`, resolved `CWD:`, resolved shell, and `PATH`, then leaves that command unexecuted. Approvals live under `~/.unlazy/approved` by default, outside the repository and shared across repositories. They bind the ledger, gate, command, expectation, resolved working directory and shell, timeout, output and regex limits, platform, and full inherited `PATH`. Changing any bound input requires approval again. Read SECURITY.md before running checks from an untrusted repository.

Count a runnable gate as met only when its process exits zero and its `EXPECT:` matches combined output. Record the resolved shell, working directory, exit status, and decisive output as evidence. Count a checked box with missing or pending evidence as unmet.

Do not silently remove an impossible gate. Add `ABANDON: <id> <non-empty reason>` and surface it in the final report. A malformed ledger, a ledger with no gates, a duplicate id, or a blank abandonment reason is an error, not completion. Read references/gates.md for the full format and authoring rules.

On this Windows machine, default `CHECK` shell is `cmd.exe` unless `UNLAZY_SHELL` is set. Prefer repository-owned scripts (`.grok/skills/unlazy` tests, `.\scripts\*.ps1`) over `grep`/`tail`.

## Grok Stop hook (already wired)

The project Stop hook already calls `.grok/hooks/bin/unlazy-stop.ps1`, which forwards the Grok Stop JSON (`sessionId`, `cwd`) to `scripts/stop-hook.mjs`.

* No scoped `.unlazy/<id>/` directory → allow. A root `GATES.md` does not arm Grok Stop, so a leftover ledger cannot trap a research turn.
* Unmet or invalid gates in that scope → `{"decision":"block","reason":"..."}` (Grok and Claude Code share this shape).
* The hook releases after six consecutive blocks with no ledger progress. Adding, renaming, or reweighting a gate is not progress; only a gate moving to met or explicitly abandoned resets the counter.
* The Stop hook does not execute `CHECK:` lines.
* Graph/loop (`stop-gate.ps1`) still runs first. Unlazy does not replace FEATURE spawn rules or `last-verify`.
* This tree may already have leftover `.unlazy/<id>/` pipelines. Unbound extra scopes fail open so they do not trap a research turn. `UNLAZY_SCOPE=<id>` selects one pipeline.

**Every fail-open is reportable.** A release after six blocks, an unbound scope that was skipped, and a scope that never armed are all silent by construction, so the ledger cannot show them. Read `.unlazy-hook-state.json` before reporting and surface each one in the final report exactly as you surface an `ABANDON`. A run that ended because enforcement gave up is not a run that passed.

Do not run install-hooks.mjs in this repo. That script writes Claude Code `.claude/settings*.json`. Grok does not use those files here (`[compat.claude] hooks = false`).

Keep `.unlazy/`, `.unlazy-hook-state.json`, and root `GATES.md` out of git (see the repo `.gitignore`).

## Pick the smallest fitting mode

* Solo: One scoped `GATES.md` for a focused task that fits one working session.
* Orchestrated: For a build or deep review, read references/method.md and references/orchestration.md. Write the contract and tree before fan-out. Give every leaf and branch its own gates file.
* Parallel: Before dispatching concurrent leaves or pipelines, also read references/parallel.md. Declare disjoint `OWNS:` paths and claim them. Treat scopes and leases as coordination, never as filesystem isolation or a security boundary.

Keep check execution sequential by default. Use `--jobs <N>` only for independent runnable gates when deterministic parallel verification saves wall-clock time. Continue printing and recording results in gate order.

## Build the Depth Tree

1. Split at natural task boundaries. Use the requested depth only while each leaf remains a coherent deliverable.
2. Give each leaf a narrow contract, exact file ownership, and its own ledger.
3. Give each branch integration gates for child verification, interface compatibility, end-to-end behavior, and regressions.
4. Dispatch only leaves whose declared dependencies are verified and whose ownership claim succeeded.
5. Re-run each returned leaf's runnable gates with `--reverify`; do not mistake `--status` for re-execution.

Use rolling dispatch: when a verified leaf unblocks another, dispatch the newly ready leaf without waiting for unrelated in-flight work. Keep states and dependencies in `PLAN.md`; append events to the scope status log.

## Work each leaf in four passes

1. Implement the complete deliverable. Leave no placeholders or deferred remainder. This pass is the one that silently does not happen, so back it with a gate over the leaf's own `OWNS:` paths rather than a promise:

   ```text
   CHECK: node .grok\skills\unlazy\scripts\no-placeholders.mjs src\layer src\io
   EXPECT: NO_PLACEHOLDERS_OK
   CONTROL: node .grok\skills\unlazy\scripts\no-placeholders.mjs src\layer --control
   ```

2. Re-read it as a domain expert and replace the cheap version of each part.
3. Hunt correctness, integration, portability, performance, and evidence defects. Fix what you find.
4. Apply low-cost polish, then repeat until a full improvement pass finds nothing.

Finish a leaf only after the pass is clean and every gate is met with evidence or visibly abandoned.

## Author gates that can fail honestly

The checker proves only the declared command oracle. It cannot infer whether an English gate title describes what the command actually measures.

* Use a decisive success-only token and require both zero exit and `EXPECT:`.
* **The `EXPECT:` token must not appear anywhere in its own `CHECK:` line.** Output is matched against the combined stream, so a token that is also an argument, flag, path, or script name can match on a usage banner, an echoed command, or an argument-parse error. Emit a distinct token from the success path only.
* **An absence gate must declare a `CONTROL:` and that control must fail before the gate is trusted.** A check that asserts nothing is wrong passes identically when it is measuring nothing at all — a moved path, a renamed marker, a scan of zero files. Prove the oracle can fail, once, then rely on it. Scripts here take `--control` for exactly this.
* Measure figures independently; do not copy a supplied number into `EXPECT:` as its own proof.
* Review consequential manual gates with evidence proportional to risk. Try to make the riskiest outcome runnable, but do not claim that manual status and risk generally correlate.
* Prefer portable Node scripts. Do not assume `grep`, `tail`, or `tr` exists on stock Windows.
* Re-run with the same declared shell and required toolchain. Treat an environment mismatch as a failed verification, not as evidence.

ka-hgis C++ examples (still quote exit codes in the reply):

```text
CHECK: powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run-ka-hgis.ps1 --smoke-quit
EXPECT: SMOKE_QUIT_OK
```

`run-ka-hgis.ps1` prints `SMOKE_QUIT_OK` only on the clean-exit path. The literal `--smoke-quit` is unusable as an expectation because it is already on the command line.

Do not put a VWorld production key in a `CHECK:` line.

## Know what this does not prove

State these limits in the report rather than letting a green ledger imply more than it earned.

* The checker proves the declared oracle, never the English title above it. A wrong gate produces a proven wrong answer. The linter narrows this; it does not close it.
* Gates constrain completeness, not quality. A leaf can pass every gate and still be poor work. Passes 2 through 4 remain unenforced by design.
* Nothing here is a security boundary. Scopes and leases coordinate cooperating agents. An agent that wants to weaken an expectation, abandon a hard gate, or edit the history file can. This is a discipline, not a seal — its value is that each of those acts leaves a visible trace.
* Every fail-open is invisible in the ledger by construction. Read `.unlazy-hook-state.json` and report releases, unbound scopes, and scopes that never armed.
* This copy assumes Windows and `cmd.exe`, PowerShell hooks, and `.grok/` paths. The `scripts/*.mjs` are portable Node; the hook wiring and shell defaults are not.

## Audit the final report

Re-measure every number and completion claim immediately before reporting. Use qualified ids such as `leaf-1.2.1:G3`. Report the measured met, unmet, and abandoned counts, surface every abandonment, and surface every fail-open the hook state records. Do not compose a done report while any required gate remains unmet.

## Spend attention where it compounds

Keep leaf briefs to the contract and one ledger. Append status instead of rewriting history. Use stronger reasoning for design, integration, and verification; use cheaper execution only for genuinely mechanical leaves. Read references/token-economy.md.