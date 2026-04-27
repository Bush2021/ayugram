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
Run `/sync-tg`. Conflicts: keep ayu side; for forked-submodule SHA conflicts, keep local SHA. Verify ayu surface intact before pushing.

### ayu/dev (AyuGramDesktop) into local dev — `rebase`
Run `/sync-ayu`. Rewrites local commits onto `ayu/dev`. Requires `git push --force-with-lease origin dev` afterward — coordinate first.

### Forked submodules — independent sync
Run `/submodule-sync`. Advance each forked submodule against its own desktop-app upstream, then register the new SHA as a separate main-repo commit. **Never** run `git submodule update --remote` in the main repo.

### Pre-sync checklist (any sync)
- Working tree clean (`git status --porcelain` empty).
- Submodules clean (`git submodule status` shows no `+`/`-`/`U` prefix).
- On the right branch (`dev`) and tracking the right remote.

## Forked submodules

These five submodules are our (or AyuGram's) forks. Their SHAs must NOT regress to desktop-app upstream during a tg merge.

| Path                  | Direct fork                       | Ultimate upstream                    |
| --------------------- | --------------------------------- | ------------------------------------ |
| `Telegram/lib_ui`     | github.com/Bush2021/lib_ui        | github.com/desktop-app/lib_ui        |
| `Telegram/lib_tl`     | github.com/Bush2021/lib_tl        | github.com/desktop-app/lib_tl        |
| `Telegram/codegen`    | github.com/Bush2021/codegen       | github.com/desktop-app/codegen       |
| `cmake`               | github.com/Bush2021/cmake_helpers | github.com/desktop-app/cmake_helpers |
| `Telegram/lib_icu`    | github.com/AyuGram/lib_icu        | github.com/desktop-app/lib_icu       |

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
| `/sync-tg`         | Merge `tg/dev` into `dev` with ayu safety checks                     |
| `/sync-ayu`        | Rebase local commits onto `ayu/dev` (rewrites history)               |
| `/submodule-sync`  | Advance each forked submodule against its own upstream, one at a time |

Each skill lives at `.claude/commands/`. Invoke via Claude Code's slash menu.
