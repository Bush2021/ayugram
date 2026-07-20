---
name: sync-ayu
description: Rebase AyuGram Desktop fork work on local dev onto ayu/dev while preserving forked submodules and the AyuGram code surface. Use when asked to sync, update, or rebase this repository against the AyuGram upstream remote. This workflow rewrites shared history and never pushes automatically.
---

# Sync AyuGram

Read `AGENTS.md` and `FORK.md` before acting. Work in the current checkout. Do not build the project.

## 1. Check preconditions

Run these checks without modifying the checkout:

- Require `git status --porcelain` to be empty.
- Require `git submodule status` to contain no line beginning with `+`, `-`, or `U`.
- Require `git branch --show-current` to be `dev`.

Stop on any failure. Do not stash, reset, clean, or otherwise alter unrelated work.

Ask the user to confirm that nobody else is currently working on `dev`, because the rebase rewrites shared history. Do not continue without explicit confirmation.

## 2. Fetch and survey

Run:

```bash
git fetch ayu
git log --oneline ayu/dev..HEAD
```

Show the local commits that will be rebased. Ask the user to confirm that the list is expected and contains neither stray commits nor AyuGram commits already incorporated another way.

## 3. Snapshot the fork surface

Create a unique directory under the system temporary directory. Record its exact path for cleanup. Save the current `HEAD` and the output of:

```bash
git ls-tree -r HEAD -- \
  Telegram/SourceFiles/ayu \
  Telegram/lib_ui Telegram/lib_tl Telegram/codegen Telegram/lib_icu cmake \
  Telegram/Resources/art/ayu Telegram/Resources/icons/ayu Telegram/Resources/qrc/ayu
```

Use PowerShell for any shell variables or temporary-path handling on Windows.

## 4. Rebase

Run:

```bash
git rebase ayu/dev
```

Resolve each conflicting commit by category:

- Forked submodule SHA conflicts: normally keep the local fork SHA, but verify it against the five-path registry and upstream relationships in `FORK.md`.
- Ayu file conflicts: merge manually so both AyuGram upstream changes and local behavior survive.
- Other files: perform a standard three-way resolution.

Stage only resolved paths and continue with `git rebase --continue`. If the resolution becomes uncertain or unsafe, stop and explain that `git rebase --abort` restores the pre-rebase state. Do not abort without user authorization if doing so would discard conflict-resolution work already performed in this run.

## 5. Verify

Re-run the `git ls-tree` snapshot command against rebased `HEAD` and compare it with the saved snapshot. Show every difference and require explicit confirmation that each change is intentional.

Then verify:

- `git submodule status` contains no `+`, `-`, or `U` state.
- Each forked submodule points to the intended fork history, not an unregistered desktop-app-only commit.
- Every AyuGram code-surface path listed in `FORK.md` remains present.

Remove only the exact temporary directory created by this run after verification.

## 6. Stop before push

Do not push from this skill. Tell the user that publishing the rebased branch requires:

```bash
git push --force-with-lease origin dev
```

State that `--force-with-lease`, never `--force`, is required and that publishing rewrites the public `dev` history.
