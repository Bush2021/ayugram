---
description: Merge tg/dev (Telegram Desktop upstream) into local dev with ayu-safety checks
allowed-tools: Bash, Read, Edit, Grep, AskUserQuestion
---

# Sync-TG — Merge tg/dev into dev

Pulls the latest tdesktop upstream changes via `git merge`, while keeping ayu code and forked-submodule SHAs intact. Reference: `FORK.md` at repo root.

## Steps

### 1. Pre-flight check

Run in parallel:
- `git status --porcelain` — must be empty
- `git submodule status` — no `+`, `-`, or `U` prefix on any line
- `git branch --show-current` — confirm branch is `dev`

If any check fails, **stop** and ask the user to clean up first. Do not proceed.

### 2. Snapshot the ayu surface

Record the pre-merge state so we can compare afterward:

```bash
git rev-parse HEAD > /tmp/sync-tg-base
git ls-tree -r HEAD -- \
  Telegram/SourceFiles/ayu \
  Telegram/lib_ui Telegram/lib_tl Telegram/codegen Telegram/lib_icu cmake \
  Telegram/Resources/art/ayu \
  Telegram/Resources/icons/ayu \
  Telegram/Resources/qrc/ayu \
  > /tmp/sync-tg-ayu-tree
```

### 3. Fetch and merge

```bash
git fetch tg
git merge tg/dev
```

If merge succeeds with no conflicts, jump to step 5.

### 4. Resolve conflicts by category

Inspect `git status` and group conflicts:

**Forked submodule SHA conflicts** (paths from `FORK.md` forked-submodule list — `Telegram/lib_ui`, `lib_tl`, `codegen`, `lib_icu`, `cmake`): always keep our side.
```bash
git checkout --ours <submodule-path>
git add <submodule-path>
```

**Ayu file conflicts** (paths under `Telegram/SourceFiles/ayu/`, `*ayu*` resource dirs, `lib_ui/ayu/`): default to keeping our side; only manually merge when the ayu code references upstream APIs that just changed.

**Other files**: standard 3-way merge resolution.

After all conflicts resolved: `git merge --continue`.

### 5. Post-merge verification

Compare against the snapshot:
```bash
git ls-tree -r HEAD -- \
  Telegram/SourceFiles/ayu \
  Telegram/lib_ui Telegram/lib_tl Telegram/codegen Telegram/lib_icu cmake \
  Telegram/Resources/art/ayu \
  Telegram/Resources/icons/ayu \
  Telegram/Resources/qrc/ayu \
  | diff /tmp/sync-tg-ayu-tree -
```

Any line of diff = something in the ayu surface changed during the merge. Show the diff to the user and require explicit confirmation that each change is intentional.

### 6. Submodule SHA sanity check

Run `git submodule status`. For each forked submodule, confirm its SHA is not a desktop-app upstream commit. If unsure, `cd` into the submodule and run `git log -1 --format='%H %s' <sha>` to see whose commit it is.

### 7. Stop short of push

Do **not** push automatically. Tell the user the merge is complete locally and they should run `git push origin dev` after their own review.
