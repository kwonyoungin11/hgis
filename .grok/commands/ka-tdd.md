---
name: ka-tdd
description: Force TDD for ka-hgis. Use when TDD, 테스트먼저, 레드그린, or /ka-tdd.
user-invocable: true
argument-hint: "<behavior>"
---

# /ka-tdd

Read `.grok/rules/32-tdd.md`.

1. Name the user-visible behavior.
2. Write **one** failing Qt Test in `tests/` (do not edit `src/` yet).
3. `ctest -R <name> -C Release --output-on-failure` — quote RED.
4. Smallest `src/` patch — quote GREEN.
5. Do not delete failing tests to pass.
