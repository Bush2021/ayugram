---
description: Merge tg/dev (Telegram Desktop upstream) into local dev with ayu-safety checks
allowed-tools: Bash, Read, Edit, Grep, AskUserQuestion
---

# Outcome

Merge current `tg/dev` into `dev` while preserving Telegram behavior, the AyuGram fork, and intentional submodule history. Finish with the local branch state defined by the repository, without building or silently publishing anything.

## Contract

Read and follow `.agents/skills/sync-tg/SKILL.md` as the canonical workflow. Also load the repository guidance it names. Choose commands and conflict-resolution techniques from the actual state instead of replaying a fixed recipe.

The skill's fork-preservation, submodule-ownership, validation, approval, and completion requirements are mandatory. Keep local completion separate from pushes and workflow dispatches. Obtain explicit approval immediately before every external write.
