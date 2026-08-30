# TDD (all ka-hgis development)

User standing order: **모든 개발은 TDD**. Slash: `/ka-tdd`.

```
NO PRODUCTION CODE WITHOUT A FAILING TEST FIRST
```

1. Write one Qt Test in `tests/` for the user-visible behavior.
2. Run it. Confirm **RED** (missing behavior, not a typo).
3. Smallest `src/` change. Confirm **GREEN**.
4. Refactor only while green.

| Change | Test |
| --- | --- |
| 단면도 / 축척자 | `tests/test_section_layout.cpp` (`section_layout_engine`) |
| 조판 | `tests/test_workflow.cpp` layout cases |
| Other | extend existing `tests/test_*.cpp` — never delete failing tests |

Hooks: `PreToolUse` denies `src/app` / `src/core` until `tests/` was written this turn.
`Stop` blocks 완료 if `src/` changed with no this-turn test write.

Exceptions (ask the user): throwaway probes you will delete; generated files; hooks/rules/docs only.
