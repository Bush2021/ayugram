---
description: Merge tg/dev (Telegram Desktop upstream) into local dev with ayu-safety checks
allowed-tools: Bash, Read, Edit, Grep, AskUserQuestion
---

# Sync-TG — Merge tg/dev into dev

Merges upstream tdesktop into `dev` while preserving ayu code and forked-submodule SHAs. See `FORK.md` for the fork structure.

## 1. Pre-flight (in parallel)

- `git status --porcelain` empty
- `git submodule status` — no `+`/`-`/`U` on any line
- `git branch --show-current` is `dev`

Any failure → stop and ask the user.

## 2. Snapshot ayu surface

```bash
git rev-parse HEAD > /tmp/sync-tg-base
git ls-tree -r HEAD -- \
  Telegram/SourceFiles/ayu \
  Telegram/lib_ui Telegram/lib_tl Telegram/codegen Telegram/lib_icu cmake \
  Telegram/Resources/art/ayu Telegram/Resources/icons/ayu Telegram/Resources/qrc/ayu \
  > /tmp/sync-tg-ayu-tree
```

## 3. Fetch and merge

```bash
git fetch tg && git merge tg/dev
```

Clean merge → skip to step 5.

## 4. Resolve conflicts by category

**Forked submodule SHAs** (the 5 paths in `FORK.md`: `Telegram/lib_ui`, `lib_tl`, `codegen`, `lib_icu`, `cmake`): judgment per submodule. First check if the desktop-app upstream is ahead:

```bash
cd <path> && git fetch upstream && git log --oneline HEAD..upstream/<branch>
```

- Upstream has new commits → rebase the fork onto upstream, resolve any fork-internal conflicts, force-push, then at repo root: `git add <path>` to stage the new fork SHA.
- Otherwise → at repo root: `git checkout --ours <path> && git add <path>`.

**Non-forked submodule SHAs** (every other submodule): take upstream's bumped SHA.

```bash
git checkout --theirs <path> && git add <path>
```

If unsure which side is newer, compare `git log` on both candidate SHAs first.

**Ayu file conflicts** (under `Telegram/SourceFiles/ayu/`, ayu resource dirs, `lib_ui/ayu/`): keep both features. Take upstream's API/structural changes; update the ayu side to match. Don't revert upstream just to preserve old ayu code.

**Version-bump conflicts** (`Telegram/SourceFiles/core/version.h`, `Telegram/Resources/winrc/Telegram.rc`, `Telegram/Resources/winrc/Updater.rc`):
- Keep ayu branding: `AppId` (the `...D666` GUID), `AppNameOld`/`AppName`/`AppFile`, `CompanyName "Radolyn Labs"`, `FileDescription`/`ProductName "AyuGram Desktop"` (or `"AyuGram Desktop Updater"`), the existing `LegalCopyright`.
- Take upstream's numbers: `AppVersion`, `AppVersionStr`, `FileVersion`, `ProductVersion`.
- Take upstream's `AppBetaVersion` as-is (true or false). Don't force `false`.

**Other files**: standard 3-way merge; same _keep both features_ rule for shared hunks.

Then `git merge --continue`.

## 5. Verify ayu surface unchanged

Re-run the `git ls-tree` from step 2 and diff against the snapshot:

```bash
git ls-tree -r HEAD -- \
  Telegram/SourceFiles/ayu \
  Telegram/lib_ui Telegram/lib_tl Telegram/codegen Telegram/lib_icu cmake \
  Telegram/Resources/art/ayu Telegram/Resources/icons/ayu Telegram/Resources/qrc/ayu \
  | diff /tmp/sync-tg-ayu-tree -
```

Any output → show the user and get explicit confirmation each change is intentional.

## 6. Submodule sanity

- `git submodule status` shows no `+`/`-`/`U`.
- For each forked submodule, verify the SHA isn't a desktop-app commit (`cd <path> && git log -1 --format='%H %s' <sha>` if unsure).
- Non-forked submodules showing `+`/`-` → `git submodule update --init <paths>` to sync the working tree. **Don't** `git add` a `+` without first checking the working-tree SHA is the one you want.

## 7. Stop short of push

Tell the user the merge is local; they run `git push origin dev` themselves.
