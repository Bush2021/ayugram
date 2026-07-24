---
name: ci-triage
description: Diagnose the newest failing GitHub Actions run for this repository by reading the actual failed job and matrix logs, classifying the root cause, and recommending the smallest next step. Use when asked to inspect, triage, or explain a recent CI failure. This is a read-only diagnostic workflow and does not implement, commit, or push fixes.
---

# Triage CI

Let the user choose the GitHub access method when they express a preference. Otherwise, choose among the available tools based on the task and current access. Keep the repository unchanged.

## 1. Select the failure

List the five or more newest workflow runs and select the newest failed run. If the result is ambiguous, retrieve structured fields and identify the run by its database ID, conclusion, workflow, branch, commit, and creation time. If no failed run appears, report that fact instead of guessing.

## 2. Read actual failed logs

Retrieve and read the failed matrix job log itself. Do not diagnose from the workflow summary, annotations, or job title alone. If the initial output is truncated or incomplete, identify the failed job and retrieve its relevant log through any available GitHub access method.

## 3. Identify the root cause

Trace the first causal error rather than the final cascade. Classify it as one of:

- Telegram or AyuGram upstream regression.
- A regression introduced by this fork's patch.
- Transient infrastructure failure such as DNS, network, runner, service, or cache instability.
- Repository or CI configuration failure.
- Unresolved when the available evidence is insufficient.

Support the classification with exact log evidence. For any upstream claim, inspect the actual upstream source, commits, workflows, or runs through an available access method; do not rely on memory.

## 4. Report the next step

If the failure is transient, say why and stop. Otherwise propose the smallest plausible fix and a focused local verification command. Respect `AGENTS.md`: do not run a project build unless the user separately confirms that they want one.

Do not edit files, commit, rerun workflows, or push unless the user explicitly asks in a follow-up.
