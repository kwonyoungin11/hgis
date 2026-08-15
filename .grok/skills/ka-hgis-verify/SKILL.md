---
name: ka-hgis-verify
description: >
  Run the ka-hgis Windows verify loop (cmake build, ctest, optional smoke-quit)
  and refuse completion claims without fresh command output. Use when the user
  says 검증, 빌드, 테스트, smoke, ctest, verify, "done", "완료", or runs /ka-hgis-verify.
when-to-use: After C++/Qt/QGIS edits, before claiming 완료/fixed/tests pass, or /ka-hgis-verify
argument-hint: "[--smoke] [target hint]"
---

# ka-hgis verify loop

Evidence before any 완료 / fixed / tests pass claim.

## Commands (this machine)

```powershell
$env:PATH = "C:\CMake\bin;" + $env:PATH
. .\scripts\dev-env.ps1
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
# UI/boot/menu path only:
.\scripts\run-ka-hgis.ps1 --smoke-quit
# full gate:
.\scripts\build-all.ps1
```

Prefer `.\scripts\build-now.ps1` only when the tree is already configured for Ninja.

## Steps

1. Name what changed (`src/app`, `src/core`, tests, scripts, docs-only).
2. If docs-only / rules-only: do **not** run a full build; say so.
3. If C++/CMake/tests changed: run **cmake --build** then **ctest**. Quote exit codes.
4. If startup/menu/UI path changed: also `--smoke-quit`.
5. After C++ edits, query `lsp` (clangd) on touched symbols when the tool is available.
6. Do not claim success from a previous turn's log.

## Fail closed

- Non-zero cmake/ctest/smoke → report the first real error; do not delete failing tests.
- Pre-existing failures: name them; do not hide them as "pass".
---
