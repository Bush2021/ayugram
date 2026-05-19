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

**Forked submodule SHA conflicts** (paths from `FORK.md` forked-submodule list — `Telegram/lib_ui`, `lib_tl`, `codegen`, `lib_icu`, `cmake`): judgment call per submodule. First check whether the fork's own desktop-app upstream has new commits:
```bash
cd <submodule-path>
git remote -v                        # confirm an upstream remote exists; add it if missing
git fetch upstream
git log --oneline HEAD..upstream/<branch>
```

If upstream has new commits, **advance the fork inline** before resolving the SHA conflict in the main repo:
```bash
git rebase upstream/<branch>         # resolve any fork-internal conflicts
git push --force-with-lease origin <branch>
cd <main-repo-root>
git add <submodule-path>             # stages the newly-advanced fork SHA
```

If upstream has no new commits (fork is already ahead), keep the local fork SHA:
```bash
cd <main-repo-root>
git checkout --ours <submodule-path>
git add <submodule-path>
```

**Non-forked submodule SHA conflicts** (every submodule path NOT in the forked list above — `lib_base`, `lib_webview`, `ThirdParty/dispatch`, etc.): take upstream's bumped SHA. Never blanket-apply the `--ours` rule here.
```bash
git checkout --theirs <submodule-path>
git add <submodule-path>
```
If unsure, `cd` into the submodule and check `git log` on both candidate SHAs to confirm upstream's is the newer commit before staging.

**Ayu file conflicts** (paths under `Telegram/SourceFiles/ayu/`, `*ayu*` resource dirs, `lib_ui/ayu/`): **keep both features**. Take upstream's structural/API changes and update the ayu side to match the new API — don't blanket-revert upstream just to preserve the old ayu code. Manually merge each file with both intents in mind.

**Other files**: standard 3-way merge resolution; same _keep both features_ principle when upstream and ayu both touched the same hunk.

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

Then sync non-forked submodule working trees to the freshly-merged superproject pointers:
```bash
git submodule update --init <non-forked-paths-that-moved>
git submodule status   # confirm no '+' or '-' prefix on any line
```
A `+` prefix after a merge means the working tree submodule HEAD differs from what the superproject records — usually a stale checkout that needs `git submodule update`, not a real change to commit. Do **not** `git add` these without first checking that the working-tree SHA is the one you actually want.

### 7. Stop short of push

Do **not** push automatically. Tell the user the merge is complete locally and they should run `git push origin dev` after their own review.
