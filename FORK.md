# Fork Management Guide

Read alongside `AGENTS.md` (upstream tdesktop conventions). This file documents what is fork-specific to `Bush2021/ayugram` and must not be merged into upstream files.

## Fork lineage

```
Bush2021/ayugram (origin, this repo)
   ^ rebase local commits onto
AyuGram/AyuGramDesktop (ayu remote)
   ^ merge into local dev
telegramdesktop/tdesktop (tg remote)
```

## Remotes

| Name     | URL                                                | Role                                |
| -------- | -------------------------------------------------- | ----------------------------------- |
| `origin` | git@github.com:Bush2021/ayugram.git                | This repo (push target)             |
| `ayu`    | git@github.com:AyuGram/AyuGramDesktop.git          | AyuGramDesktop upstream             |
| `tg`     | git@github.com:telegramdesktop/tdesktop.git        | Telegram Desktop upstream           |
| `pr`     | git@github.com:Bush2021/AyuGramDesktop.git         | Personal AyuGramDesktop fork (PRs)  |

## Sync workflows

### tg/dev (tdesktop) into local dev — `merge`
Run `/sync-tg`. Conflict resolution is judgment-based — the rule of thumb is **keep both features**: preserve ayu functionality while taking upstream's new behavior. When upstream changes an API that ayu code calls, update the ayu code to match the new API rather than reverting upstream's change. For SHA conflicts, decide per submodule (see _Forked submodules_ below).

If during `/sync-tg` a forked submodule has new commits on its desktop-app upstream, advance the fork inline: cd into the submodule, rebase its branch onto upstream, force-push to our fork, then continue the main-repo merge with the new fork SHA staged. Verify the ayu surface is intact before pushing.

### ayu/dev (AyuGramDesktop) into local dev — `rebase`
Run `/sync-ayu`. Rewrites local commits onto `ayu/dev`. Requires `git push --force-with-lease origin dev` afterward — coordinate first.

### vs2026 branch — rebase onto dev after every dev change
The `vs2026` branch carries VS 2026 build adjustments on top of `dev` (currently one commit). After any work that advances `dev` (a sync, a feature commit, anything), refresh `vs2026`:

```bash
git checkout vs2026
git rebase dev
git push --force-with-lease origin vs2026
git checkout dev
```

Resolve any rebase conflicts the same way as a normal dev edit. The vs2026 commit should stay a thin shim on top — if it grows, fold the non-VS-specific parts back into dev.

### Pre-sync checklist (any sync)
- Working tree clean (`git status --porcelain` empty).
- Submodules clean (`git submodule status` shows no `+`/`-`/`U` prefix).
- On the right branch (`dev`) and tracking the right remote.

## Forked submodules

These five submodules are our (or AyuGram's) forks.

| Path                  | Direct fork                       | Ultimate upstream                    |
| --------------------- | --------------------------------- | ------------------------------------ |
| `Telegram/lib_ui`     | github.com/Bush2021/lib_ui        | github.com/desktop-app/lib_ui        |
| `Telegram/lib_tl`     | github.com/Bush2021/lib_tl        | github.com/desktop-app/lib_tl        |
| `Telegram/codegen`    | github.com/Bush2021/codegen       | github.com/desktop-app/codegen       |
| `cmake`               | github.com/Bush2021/cmake_helpers | github.com/desktop-app/cmake_helpers |
| `Telegram/lib_icu`    | github.com/AyuGram/lib_icu        | github.com/desktop-app/lib_icu       |

When `/sync-tg` reports an SHA conflict on one of these, decide per submodule: either keep the local fork SHA (if the fork is ahead of its desktop-app upstream and upstream has no new commits to fold in), or advance the fork first — rebase onto its desktop-app upstream, force-push to our fork, then continue the main-repo merge with the new fork SHA. **Never** run `git submodule update --remote` in the main repo — it overwrites hand-picked SHAs with whatever the tracking branch tip happens to be.

All other (~23) submodules point straight at desktop-app or third-party upstreams; let them flow with normal SHA bumps.

## Ayu code surface

These paths host AyuGram-specific code/assets. Verify they survive every merge:

- `Telegram/SourceFiles/ayu/` — entire tree (settings, features, data, ui, infrastructure)
- `Telegram/lib_ui/ayu/` — inside the forked lib_ui submodule
- `Telegram/Resources/art/ayu/`, `icons/ayu/`, `qrc/ayu/` — themed assets
- `Telegram/SourceFiles/**/*.style` — entries with `ayu_` prefix (mixed into upstream style files)
- `Telegram/CMakeLists.txt` — every line that mentions `ayu/`

## Hands-off list (owned by tg/ayu upstream)

Editing these in this repo causes recurring merge conflicts. Put fork-specific rules here in `FORK.md` or in the relevant skill — not in upstream files.

- `AGENTS.md`, `REVIEW.md`, `.editorconfig`
- `Telegram/CMakeLists.txt` outside the ayu blocks
- All non-forked submodule directories

## Skills

| Skill              | Purpose                                                              |
| ------------------ | -------------------------------------------------------------------- |
| `/sync-tg`         | Merge `tg/dev` into `dev` with ayu safety checks; advance forked submodules inline when their upstreams have new commits |
| `/sync-ayu`        | Rebase local commits onto `ayu/dev` (rewrites history)               |

Each skill lives at `.claude/commands/`. Invoke via Claude Code's slash menu.

## Git & Submodules
- Never use `--ours` or `--theirs` blindly during merge conflict resolution involving submodules; inspect each submodule SHA and confirm which side is canonical before committing.
- Distinguish forked submodules from upstream-tracked ones before amending SHAs. Ask if unclear.
- After any merge involving submodules, verify all submodule pointers and run a build before declaring success.
