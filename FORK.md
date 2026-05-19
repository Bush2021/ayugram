# Fork Management

This is `Bush2021/ayugram`, a fork of `AyuGram/AyuGramDesktop`, which forks `telegramdesktop/tdesktop`. Read alongside `AGENTS.md`.

## Remotes

| Name     | URL                                          | Role                                |
| -------- | -------------------------------------------- | ----------------------------------- |
| `origin` | github.com/Bush2021/ayugram                  | This repo (push target)             |
| `ayu`    | github.com/AyuGram/AyuGramDesktop            | AyuGramDesktop upstream             |
| `tg`     | github.com/telegramdesktop/tdesktop          | Telegram Desktop upstream           |
| `pr`     | github.com/Bush2021/AyuGramDesktop           | Personal AyuGramDesktop fork (PRs)  |

## Sync workflows

- **`/sync-tg`** — merge `tg/dev` into `dev`. Conflict rule: **keep both features** — preserve ayu, update ayu code to match upstream API changes; don't revert upstream.
- **`/sync-ayu`** — rebase local commits onto `ayu/dev`. Requires `git push --force-with-lease origin dev` after.
- **`vs2026` branch** — thin shim of VS 2026 build adjustments on top of `dev`. After any change to `dev`:
  ```bash
  git checkout vs2026 && git rebase dev && git push --force-with-lease origin vs2026 && git checkout dev
  ```
  If vs2026 grows beyond build adjustments, fold the non-VS-specific parts into `dev`.

## Forked submodules

These five point at our (or AyuGram's) fork — not desktop-app directly:

| Path                | Direct fork                       | Ultimate upstream                    |
| ------------------- | --------------------------------- | ------------------------------------ |
| `Telegram/lib_ui`   | github.com/Bush2021/lib_ui        | github.com/desktop-app/lib_ui        |
| `Telegram/lib_tl`   | github.com/Bush2021/lib_tl        | github.com/desktop-app/lib_tl        |
| `Telegram/codegen`  | github.com/Bush2021/codegen       | github.com/desktop-app/codegen       |
| `cmake`             | github.com/Bush2021/cmake_helpers | github.com/desktop-app/cmake_helpers |
| `Telegram/lib_icu`  | github.com/AyuGram/lib_icu        | github.com/desktop-app/lib_icu       |

SHA-conflict policy: per submodule, either keep the fork SHA (if the fork is already ahead of its desktop-app upstream) or advance the fork first by rebasing onto upstream and force-pushing. Procedure lives in `/sync-tg`.

All other ~23 submodules point straight at desktop-app or third-party — let them flow with upstream bumps.

**Never** run `git submodule update --remote` in the main repo. It overwrites hand-picked SHAs with whatever the tracking branch tip happens to be.

## Ayu code surface

Every merge must preserve these paths:

- `Telegram/SourceFiles/ayu/`
- `Telegram/lib_ui/ayu/` (inside the forked lib_ui)
- `Telegram/Resources/art/ayu/`, `icons/ayu/`, `qrc/ayu/`
- `Telegram/SourceFiles/**/*.style` — entries with the `ayu_` prefix (mixed into upstream style files)
- `Telegram/CMakeLists.txt` — lines mentioning `ayu/`

## Hands-off (owned by tg/ayu upstream)

Editing these in this repo causes recurring merge conflicts. Put fork-specific rules in `FORK.md` or a skill, not these files:

- `AGENTS.md`, `REVIEW.md`, `.editorconfig`
- `Telegram/CMakeLists.txt` outside ayu blocks
- All non-forked submodule directories
