# AyuGram (Bush2021's fork)

![AyuGram Logo](.github/AyuGram.png)
![AyuChan](.github/AyuChan.png)

[ English | [Русский](README-RU.md) ]

A personal fork of [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop),
which is based on [Telegram Desktop](https://github.com/telegramdesktop/tdesktop).
Bush2021 maintains this fork to bring in upstream changes promptly while keeping
AyuGram features and adding features for personal use. Merge conflicts are resolved
with those features in mind, and changes are built on GitHub Actions runners.

Development builds are available from [Actions](#development-builds).
[Releases](https://github.com/Bush2021/ayugram/releases) follow upstream version
releases. Windows users can choose between Qt 5 and Qt 6 builds.

## What this fork adds

### Windows Qt 6 builds

The Qt 6 build targets Windows 10 and later. It makes the Qt RHI renderer available
on Windows x64, with a Direct3D 11 rendering path on supported hardware. It also
includes adjustments to CJK font fallback and fixes for Qt 6 popup menu behavior.
A separate Qt 5 build remains available for older Windows versions.

Translation language preferences are stored by language name so they survive
switching between the Qt 5 and Qt 6 builds.

### OpenAI translation

Use OpenAI or a compatible service for message translation. The model, API endpoint,
API key, system prompt, and translation prompt are configurable. Both Responses and
Chat Completions endpoints are supported.

The API key is saved in the application's encrypted local settings. Selecting an
external translation provider prompts you to confirm where message text will be sent.

### Privacy and external requests

This fork removes AyuGram's remote configuration service and crash report upload
paths. Built-in automatic updates are disabled; download updates from this repository.

- External translation, registration date lookup, GIF search, and nearby place
  search show disclosures before sending data to the corresponding service or bot.
- Fetching missing music artwork from Apple's iTunes Search service is off by
  default. The setting explains which track information is sent when enabled.
- AyuGram language packs are cached, with network refreshes limited to once per day.

### Other changes

- Ads have three modes: Show Ads, Camouflage, and Strict Block. Camouflage requests
  ads but hides them; Strict Block skips ad requests.
- Compatibility fixes preserve AyuGram settings as upstream UI code changes,
  including the gift button toggle and filtering out sticker packs you have not added.
- The build tooltip includes the source commit hash to help identify development builds.

### Secret chats (development builds)

Secret chats were added after v7.2.9 and are available in development builds.
They include text and media messages, encryption key comparison, self-destruct
timers, and encrypted local storage. The v7.2.9 release does not include this feature.

## Downloads and updates

### Development builds

Choose a workflow and branch below, open its latest successful run, then download
the named artifact from the **Artifacts** section. Sign in to GitHub to download
workflow artifacts. Extract the whole archive; Windows builds include a `modules`
folder that belongs alongside the executable.

| Build | Workflow and branch | Artifact |
| --- | --- | --- |
| Windows x64, Qt 6 (Windows 10+) | [Windows / vs2026](https://github.com/Bush2021/ayugram/actions/workflows/win.yml?query=branch%3Avs2026) | `AyuGram x64 qt6 Ninja Multi-Config` |
| Windows x64, Qt 5 | [Windows / dev](https://github.com/Bush2021/ayugram/actions/workflows/win.yml?query=branch%3Adev) | `AyuGram x64 Ninja Multi-Config` |
| macOS, universal | [macOS / dev](https://github.com/Bush2021/ayugram/actions/workflows/mac.yml?query=branch%3Adev) | `AyuGram` |

The two Windows builds share a workflow name, so check the branch before downloading.
Development builds can contain features added since the latest release. Artifacts
expire, so use a recent successful run if an older download is unavailable.

### Releases

Download versioned packages from [Releases](https://github.com/Bush2021/ayugram/releases).
The Windows Qt 6 package has `_Qt6_win10+` in its filename; the other Windows x64
package uses Qt 5. macOS packages have `_macOS_universal` in their filenames.

Releases follow upstream releases. For newer changes between releases, use the
Actions builds above. All of these builds require manual updates.

### Building from source

See the build instructions for [Windows](docs/building-win.md),
[macOS](docs/building-mac.md), and [Linux](docs/building-linux.md).
Use the `vs2026` branch for this fork's Windows Qt 6 build configuration.

For upstream AyuGram packages distributed through Winget, Scoop, Homebrew, or Linux
package repositories, see the [upstream download instructions](https://github.com/AyuGram/AyuGramDesktop#downloads).
The downloads for Bush2021's fork are the Actions artifacts and Releases linked above.

## Features inherited from AyuGram

- Configurable ghost mode
- Deleted and edited message history, with anti-recall
- Font and appearance customization
- Streamer mode
- Local Telegram Premium appearance
- Message translation
- Media preview and quick reactions with force click on macOS

See the [AyuGram documentation](https://docs.ayugram.one/desktop/) for shared features.
The screenshots below show AyuGram's interface and shared settings.

<h3>
  <details>
    <summary>Preview</summary>
    <table>
      <tr>
        <td><img src='.github/demos/demo1.png' width='268' alt='Preferences'></td>
        <td><img src='.github/demos/demo2.png' width='268' alt='AyuGram Options'></td>
        <td><img src='.github/demos/demo3.png' width='268' alt='Message Filters'></td>
      </tr>
      <tr>
        <td><img src='.github/demos/demo4.png' width='268' alt='Appearance'></td>
        <td><img src='.github/demos/demo5.png' width='268' alt='Chats'></td>
      </tr>
    </table>
  </details>
</h3>

## Support upstream

AyuGram's [donation page](https://docs.ayugram.one/donate/) supports the upstream
AyuGram project. The artwork and shared features in this fork come from AyuGram
and its contributors.

## Credits

### Telegram clients

- [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop)
- [Telegram Desktop](https://github.com/telegramdesktop/tdesktop)
- [Kotatogram](https://github.com/kotatogram/kotatogram-desktop)
- [64Gram](https://github.com/TDesktop-x64/tdesktop)
- [Forkgram](https://github.com/forkgram/tdesktop)

### Libraries used

- [JSON for Modern C++](https://github.com/nlohmann/json)
- [SQLite](https://github.com/sqlite/sqlite)
- [sqlite_orm](https://github.com/fnc12/sqlite_orm)
- [androidx sources](https://github.com/androidx/androidx)

### Icons

- [Solar Icon Set](https://www.figma.com/community/file/1166831539721848736)

### Bots

- [TelegramDB](https://t.me/tgdatabase) for username lookup by ID (until closing free inline mode at 2 April 2026)
