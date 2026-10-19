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
- **`/sync-ayu`** — reconstruct durable fork intent on the latest `ayu/dev`. It drops upstream-equivalent and temporary history, preserves merge-only behavior, reconciles fork submodules, and requires exact `--force-with-lease` pushes after verification.

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

## Fork feature rules

- **Document every feature.** A change that adds or changes a user-visible feature of this fork also updates the "What this fork adds" section of `README.md` in the same change. `README-RU.md` is upstream AyuGram's text; leave it alone.
- **Copyright.** A new source file this fork adds starts with the AyuGram header, credited to Bush2021 with the year the file was created. Keep the existing tdesktop and Radolyn headers on files that came from upstream. A file written for a PR to AyuGram (sent through the `pr` remote) uses `Copyright @Radolyn` instead, like the OpenAI translator files.
  ```cpp
  // This is the source code of AyuGram for Desktop.
  //
  // We do not and cannot prevent the use of our code,
  // but be respectful and credit the original author.
  //
  // Copyright @Bush2021, 2026
  ```

## Fork-owned (not merged from upstream)

- `.github/ISSUE_TEMPLATE/`: this fork's own issue forms and support policy. On conflict keep ours whole. Do not fold upstream template changes in.
- `.github/CONTRIBUTING.md`: this fork's contribution policy. Pull requests are not accepted. On conflict keep ours whole.
## Hands-off (owned by tg/ayu upstream)

Editing these in this repo causes recurring merge conflicts. Put fork-specific rules in `FORK.md` or a skill, not these files:

- `AGENTS.md`, `REVIEW.md`, `.editorconfig`
- `Telegram/CMakeLists.txt` outside ayu blocks
- All non-forked submodule directories
