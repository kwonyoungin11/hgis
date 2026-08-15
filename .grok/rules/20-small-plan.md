# Small plan before any develop or fix

Before editing code for a feature or bug:

1. Write a short plan in the reply (or a tiny plan file only if the change spans several modules).
2. Then implement only that plan.

## Plan size

- 5–10 lines. Not a design document.
- GIS items must name the GIS object: project CRS, layer CRS, OTF, canvas order, WMS/GetMap, edit buffer, scale.

## Plan must include

- Symptom the user can see
- GIS/code cause (verified, not guessed)
- Files to touch
- Done check the user can see (one sentence)

## Do not

- Start coding before the plan is written
- Expand the plan mid-flight into unrelated UI/refactors
- Ask the user to fill GIS internals; they only confirm the done check
