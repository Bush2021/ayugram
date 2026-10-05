---
name: sync-tg
description: Merge Telegram Desktop upstream tg/dev into local dev while preserving AyuGram features, branding, resources, and forked-submodule histories. Use when asked to sync, update, or merge this repository with Telegram Desktop upstream and carry the result through the vs2026 branch and optional CI dispatch.
---

# Sync Telegram Desktop

## Outcome

Merge current `tg/dev` into `dev` without losing Telegram changes or the AyuGram fork. Leave every submodule at an intentional commit, keep `vs2026` as a thin branch on the new `dev`, and clearly distinguish local completion from publication. Do not build.

Read `AGENTS.md` and `FORK.md` first. Treat `FORK.md` as the source of truth for remotes, protected fork surfaces, forked submodules, and branch roles.

## Safety boundary

- Start only from clean `dev` with all submodules initialized and aligned. Stop rather than stash, reset, clean, or disturb unrelated work.
- Capture enough of the starting fork surface to prove afterward that every change there is intentional.
- Never use `git submodule update --remote`.
- Any external write requires explicit approval immediately before it. A force-push must use an exact lease and identify the remote and ref being rewritten.

## Merge judgment

- Perform a real merge from `tg/dev` into `dev`.
- Preserve both features in shared conflicts. Adapt AyuGram code to Telegram's newer APIs and structure instead of restoring stale whole files or reverting upstream behavior.
- Preserve AyuGram identity and branding. Take Telegram's current version and beta values.
- For each registered forked submodule, decide from ancestry and repository ownership. Keep the fork pointer when it already carries the required upstream history. If the fork itself must advance, update and verify that fork before recording its new commit in the parent.
- Let non-forked submodules follow Telegram's intended commits after verifying ambiguous ancestry.
- Stage only paths whose resolution has been inspected.

## Verification and publication

The merge is locally complete only when:

- the merge contains the intended Telegram parent and preserves the AyuGram behavior;
- every protected-surface difference from the starting snapshot is reviewed and intentional;
- all submodules are clean, aligned with their recorded commits, and forked pointers belong to the intended fork histories;
- the parent worktree is clean and the checkout has returned to `dev`;
- `vs2026` has been rebased onto the finalized `dev` while retaining only its Visual Studio compatibility delta.

Publish only the parts the user approves. Treat pushes of forked submodules, `dev`, and the rewritten `vs2026` as separate external effects. Offer the repository's macOS and Windows workflow dispatches after publication, but run them only with fresh approval. Report which branches and workflows remain local, published, dispatched, or unverified.
