---
name: release-notes
description: Write release notes for a published Bush2021/ayugram release. Links the matching Telegram Desktop release, any new AyuGram releases, and summarizes this fork's own commits. Use after a release is published.
---

# Release Notes

Argument: tag (default: latest release). Compare against the previous release.

GitHub Actions creates release tags on `origin`, so a local tag of the same name usually came from `tg` and points elsewhere. Resolve both tags with `git ls-remote origin refs/tags/<tag>` and compare those SHAs, never the local tags.

- Link the Telegram Desktop release with the same tag, and AyuGram releases published since the previous release.
- List fork-only commits (not in `tg/dev` or `ayu/dev`), plus new Bush2021 commits in forked submodules. Those submodules are rebased, so skip rebased copies of old commits.
- Keep user-visible changes only. One line each, linked to its commit.

```markdown
## Upstream
- Telegram Desktop TAG: <link>
- AyuGram vX.Y.Z: <link>

## Features
- <change> ([abc1234](<commit link>))

## Fixes
- <fix> ([abc1234](<commit link>))

**Full Changelog**: https://github.com/Bush2021/ayugram/compare/PREV...TAG
```

Show the draft. Update the release only after approval.
