---
description: Advance forked submodule SHAs against their own upstreams; never use --remote in main repo
allowed-tools: Bash, Read, AskUserQuestion
---

# Submodule-Sync — Independent submodule advance

Five submodules in this repo are forks (lib_ui, lib_tl, codegen, cmake, lib_icu). Their SHAs must be advanced *independently* against their own desktop-app upstreams; do **not** rely on the main repo's merge to update them. Reference: `FORK.md`.

## Critical guardrail

**Never** run `git submodule update --remote` (or any batch submodule advance) in the main repo. It overwrites hand-picked SHAs with whatever happens to be at the tracking branch tip, including for non-forked submodules.

## Submodules to handle

| Path                  | Fork remote (`origin` in submodule) | Upstream to fetch                    |
| --------------------- | ----------------------------------- | ------------------------------------ |
| `Telegram/lib_ui`     | Bush2021/lib_ui                     | github.com/desktop-app/lib_ui        |
| `Telegram/lib_tl`     | Bush2021/lib_tl                     | github.com/desktop-app/lib_tl        |
| `Telegram/codegen`    | Bush2021/codegen                    | github.com/desktop-app/codegen       |
| `cmake`               | Bush2021/cmake_helpers              | github.com/desktop-app/cmake_helpers |
| `Telegram/lib_icu`    | AyuGram/lib_icu                     | github.com/desktop-app/lib_icu       |

## Steps (per submodule)

Ask the user which submodule to sync, or offer to walk through them one-by-one. For each:

### 1. Enter and verify state

```bash
cd <submodule-path>
git status                # must be clean
git branch --show-current # confirm tracking branch (usually master or dev)
```

### 2. Ensure upstream remote exists

If `git remote -v` doesn't show a remote pointing at the desktop-app upstream, add it:
```bash
git remote add upstream https://github.com/desktop-app/<repo>.git
```
(Use the table above for the repo name.)

### 3. Fetch and merge upstream

```bash
git fetch upstream
git merge upstream/<branch>     # default 3-way merge, branch usually master
```

Resolve conflicts manually. The submodule's own ayu modifications (if any) live alongside upstream code without a clear directory boundary, so apply judgement.

### 4. Push to the fork

```bash
git push origin <branch>
```

### 5. Register the new SHA in main repo

Return to main repo:
```bash
cd <main-repo-root>
git add <submodule-path>
git commit -m "Bump <submodule-path> to <new-sha-short>"
```

### 6. Confirm before continuing to next

Ask the user explicitly before moving on to the next submodule. Do not batch.

## After all submodules done

Run `git submodule status` once more to confirm clean state across the board. Suggest the user push the SHA-bump commits to `origin` when ready.
