---
name: ci-triage
description: Diagnose the newest failing GitHub Actions run for this repository by reading the actual failed job and matrix logs, classifying the root cause, and recommending the smallest next step. Use when asked to inspect, triage, or explain a recent CI failure. This is a read-only diagnostic workflow and does not implement, commit, or push fixes.
---

# Triage CI

## Outcome

Identify the first causal failure in the newest relevant failed GitHub Actions run. Explain what failed, classify its ownership, and recommend the smallest justified next step. Leave both the repository and GitHub unchanged.

## Judgment

- Honor the user's preferred GitHub access method. Otherwise use the best available source of structured run data and complete logs.
- Identify the run precisely. If there is no failure or the intended run is ambiguous, report that instead of guessing.
- Read the actual failed job or matrix log. Summaries, annotations, and job names are navigation aids, not sufficient evidence.
- Trace backward from cascaded errors to the first actionable cause.
- Classify the cause as upstream, fork regression, CI configuration, transient infrastructure, or unresolved.
- Verify upstream claims against current upstream source, commits, workflows, or runs.
- Stop at unresolved when the evidence cannot support a stronger conclusion.

## Completion contract

Report the run and failed job, the root cause with concise log evidence, the classification and confidence, and the smallest next step. Include focused verification for a non-transient fix. Do not edit files, rerun workflows, commit, push, or build. A build still requires separate user confirmation under `AGENTS.md`.
