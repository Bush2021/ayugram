---
description: Diagnose the latest failing CI run
allowed-tools: Bash, Read, Grep, AskUserQuestion
---

# Outcome

Diagnose the newest relevant failing CI run from its actual failed-job evidence. Return the root cause, ownership classification, and smallest justified next step without changing local or remote state.

## Contract

Read and follow `.agents/skills/ci-triage/SKILL.md` as the canonical workflow. Choose the available tools and commands that best fit the current GitHub access and failure shape. The skill's evidence, read-only, stop, and completion requirements are mandatory. Its implementation details are not a fixed command sequence.
