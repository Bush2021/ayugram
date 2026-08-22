---
name: ci-triage
description: Diagnose the newest failing GitHub Actions run for this repository by reading the actual failed job and matrix logs, classify the root causes, fix everything the run exposes, commit and push, and re-trigger the full CI matrix. Use when asked to inspect, triage, explain, or clear a recent CI failure. All build verification happens in CI, never locally.
---

# Triage CI

## Outcome

Identify every defect the newest relevant failed GitHub Actions run exposes, fix them, land the fixes, and put them back through CI. Verification belongs to CI. Do not build locally to check a fix.

## Judgment

- Identify the run precisely. If there is no failure or the intended run is ambiguous, report that instead of guessing.
- Read the actual failed job or matrix log. Summaries, annotations, and job names are navigation aids, not sufficient evidence.
- Trace backward from cascaded errors to the first actionable cause.
- Classify the cause as upstream, fork regression, CI configuration, transient infrastructure, or unresolved.
- Verify upstream claims against current upstream source, commits, workflows, or runs.
- Stop at unresolved when the evidence cannot support a stronger conclusion.

## Fix

- Fix everything the run exposes, not just the failure that stopped the build. Read every failed job and every matrix leg first, collect the distinct defects across all of them, then fix them together in one pass.
- Tracing cascaded errors back to their actionable cause applies per defect. It is not a reason to stop after one.
- Fix only what a log proves. Do not trust a text search to find sibling instances elsewhere in the tree, because locals, aliases, and templates hide the shape, and do not speculatively change sites CI has not flagged.
- A stopped build hides whatever sits behind it, so the next run exposing new defects is normal progress, not a failed fix.
- Upstream-owned code gets the smallest change that compiles, matching the surrounding style. Say plainly in the report that `/sync-tg` will conflict there once upstream fixes it themselves.
- Do not weaken a workflow, disable a warning, or relax a build setting to make a failure disappear.

## Commit, push, and re-trigger=

Carry the fix onto the build shim branch, per `FORK.md`:

Confirm with `git range-diff origin/vs2026...vs2026` that the shim commits replayed unchanged before force-pushing.

Then trigger all four build workflows:

| Workflow         | Ref      |
| ---------------- | -------- |
| `Windows`        | `dev`    |
| `macOS`          | `dev`    |
| `macOS packaged` | `dev`    |
| `Windows`        | `vs2026` |

```
gh workflow run "<workflow>" --repo Bush2021/ayugram --ref <ref>
```

## Iterate

Each round clears what the previous one could reach before the build stopped. When a triggered run fails again, run the whole loop from the top — read every failed job, fix everything they expose, commit, push, rebase, trigger — without asking for confirmation each round. Keep going until the matrix is green or a stop condition below applies.

## Stop and report instead

- The evidence cannot support a root cause.
- The fix needs a design decision, changes behavior, or touches the ayu code surface in `FORK.md`.
- The failure is transient infrastructure, where a re-run is the whole fix.

## Completion contract

Report the run and every failed job, each root cause with concise log evidence, the classification and confidence, what was changed and why, and the URLs of the triggered runs.
