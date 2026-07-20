---
name: sync-tg
description: Merge Telegram Desktop upstream tg/dev into local dev while preserving AyuGram features, branding, resources, and forked-submodule histories. Use when asked to sync, update, or merge this repository with Telegram Desktop upstream and carry the result through the vs2026 branch and optional CI dispatch.
---

# Sync Telegram Desktop

Read `AGENTS.md` and `FORK.md` before acting. Work in the current checkout. Preserve both upstream behavior and the AyuGram code surface. Do not build the project.

## 1. Check preconditions

Run these checks in parallel where practical:

- Require `git status --porcelain` to be empty.
- Require `git submodule status` to contain no line beginning with `+`, `-`, or `U`.
- Require `git branch --show-current` to be `dev`.

Stop on any failure. Do not stash, reset, clean, or alter unrelated work.

## 2. Snapshot the fork surface

Create a unique directory under the system temporary directory. Record its exact path for cleanup. Save the current `HEAD` and the output of:

```bash
git ls-tree -r HEAD -- \
  Telegram/SourceFiles/ayu \
  Telegram/lib_ui Telegram/lib_tl Telegram/codegen Telegram/lib_icu cmake \
  Telegram/Resources/art/ayu Telegram/Resources/icons/ayu Telegram/Resources/qrc/ayu
```

Use PowerShell for shell variables and temporary-path handling on Windows.

## 3. Fetch and merge

Run:

```bash
git fetch tg
git merge tg/dev
```

For a clean merge, continue with verification. For conflicts, resolve them by category.

## 4. Resolve conflicts

### Forked submodule SHAs

For each of the five forked paths in `FORK.md`, inspect the actual repositories and compare both candidate SHAs. Fetch its configured desktop-app upstream and inspect `HEAD..upstream/<branch>`.

- If desktop-app upstream has new commits, rebase the registered fork branch onto that upstream. Resolve fork-internal conflicts. Before force-pushing, show the exact fork remote and branch and obtain explicit user confirmation. Use `--force-with-lease`, then stage the new submodule SHA at the main-repository root.
- Otherwise, keep the local main-repository side with `git checkout --ours <path>` and stage that path.

Never replace a registered fork SHA with an unverified desktop-app-only SHA.

### Non-forked submodule SHAs

Take Telegram upstream's bumped SHA with `git checkout --theirs <path>` and stage it. If either side's ordering is unclear, inspect both candidate SHAs before choosing.

Never run `git submodule update --remote`.

### AyuGram files

For conflicts under the AyuGram code surface in `FORK.md`, keep both features. Preserve the AyuGram behavior while adapting it to Telegram upstream API and structural changes. Do not revert upstream changes merely to retain old AyuGram code.

### Version and branding files

For conflicts in `Telegram/SourceFiles/core/version.h`, `Telegram/Resources/winrc/Telegram.rc`, and `Telegram/Resources/winrc/Updater.rc`:

- Keep AyuGram branding: the `...D666` `AppId`, `AppNameOld`, `AppName`, `AppFile`, `CompanyName "Radolyn Labs"`, `FileDescription`, `ProductName`, and existing `LegalCopyright`.
- Take Telegram upstream's `AppVersion`, `AppVersionStr`, `FileVersion`, and `ProductVersion` numbers.
- Take upstream's `AppBetaVersion` unchanged, whether true or false.

### Other files

Perform a standard three-way merge and preserve both features in shared hunks. Stage only resolved paths. Finish with `git merge --continue`.

## 5. Verify the fork surface

Re-run the `git ls-tree` snapshot command against merged `HEAD` and compare it with the saved snapshot. Show every difference and require explicit confirmation that each one is intentional.

## 6. Verify submodules

- Require `git submodule status` to contain no `+`, `-`, or `U` state.
- Verify every forked submodule SHA belongs to the intended fork history.
- For non-forked submodules with a `+` or `-` working-tree state, run `git submodule update --init <exact-paths>` to match the recorded SHAs. Never stage a `+` state without first proving that its working-tree SHA is intended.

Remove only the exact temporary directory created by this run after verification.

## 7. Publish dev only with confirmation

State that the merge is local. Ask whether the user wants Codex to run:

```bash
git push origin dev
```

Do not push without explicit confirmation.

## 8. Update vs2026

After `dev` is finalized, run:

```bash
git checkout vs2026
git rebase dev
```

Ask for explicit confirmation before `git push --force-with-lease origin vs2026`. Return to `dev` after the branch work, whether the user approves the push or leaves it local.

## 9. Offer CI dispatch

Ask whether to trigger these workflows with `gh`:

```bash
gh workflow run mac.yml --ref dev
gh workflow run win.yml --ref dev
gh workflow run win.yml --ref vs2026
```

Run them only after explicit confirmation. Report the dispatched runs or any `gh` errors. Do not substitute a local build.
