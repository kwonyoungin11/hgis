# STA contracts (Cursor)

## Parent → STA

```json
{
  "task_id": "task_YYYYMMDD_NNN",
  "query": "",
  "constraints": {
    "min_steps": 4,
    "max_steps": 12,
    "allowed_tools": ["Read", "Grep", "Glob", "WebFetch"],
    "report_interval_steps": 2
  },
  "context": {
    "target_files": [],
    "known_facts": ""
  }
}
```

## Pulse (STA transcript)

`[PROGRESS] Step 3/7: <one line>`

## STA → parent (final only)

```json
{
  "taskId": "task_YYYYMMDD_NNN",
  "status": "COMPLETED",
  "totalStepsTaken": 6,
  "branchesExplored": ["main"],
  "finalConclusion": "",
  "rejectedHypotheses": [],
  "actionablePlan": []
}
```
