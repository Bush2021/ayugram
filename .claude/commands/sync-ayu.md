---
description: Rebase local commits onto ayu/dev (AyuGramDesktop upstream); rewrites history
allowed-tools: Bash, Read, Edit, Grep, AskUserQuestion
---

# Sync-Ayu — Rebase onto ayu/dev

Pulls AyuGramDesktop upstream changes by rebasing local commits onto `ayu/dev`. **This rewrites history that may already be pushed to `origin`** — coordinate before pushing afterward. Reference: `FORK.md`.

## Steps

### 1. Pre-flight check

Same as `/sync-tg` step 1: clean working tree, clean submodules, on `dev`.

Then ask the user explicitly: "Confirm no one else is currently working on `dev` (rebase will rewrite shared history)." Do not proceed without confirmation.

### 2. Fetch and survey

```bash
git fetch ayu
git log --oneline ayu/dev..HEAD
```

Show the user the list of local commits that will be rebased. Ask them to confirm the list is what they expect (no stray commits, no ayu commits already merged in another way).

### 3. Snapshot ayu surface

Same as `/sync-tg` step 2 (write to `/tmp/sync-ayu-base` and `/tmp/sync-ayu-ayu-tree`).

### 4. Rebase

```bash
git rebase ayu/dev
```

For each conflicting commit, classify:

**Forked submodule SHA conflicts**: usually keep our side (our forked submodules are typically further ahead than what ayu has registered). Verify against the SHA-registry expectations from `FORK.md`.

**Ayu file conflicts**: case by case. If both ayu upstream and we edited the same ayu file, manual merge.

**Other files**: standard 3-way.

```bash
git add <files>
git rebase --continue
```

If you get lost mid-rebase, `git rebase --abort` returns to the pre-rebase state safely.

### 5. Post-rebase verification

Same as `/sync-tg` step 5–6: diff the ayu surface against the snapshot, sanity-check submodule SHAs.

### 6. Push instructions (do not push automatically)

Tell the user:
```bash
git push --force-with-lease origin dev
```

Explicitly mention `--force-with-lease` (not `--force`) and that this rewrites public history. Do not run the push from the skill.
