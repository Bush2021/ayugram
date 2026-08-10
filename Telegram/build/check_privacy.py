#!/usr/bin/env python3

from pathlib import Path
import sys


REPOSITORY = Path(__file__).resolve().parents[2]
RUNTIME_ROOTS = (
    REPOSITORY / "Telegram" / "SourceFiles",
    REPOSITORY / "Telegram" / "Resources",
)
RUNTIME_FILES = (
    REPOSITORY / "CMakeLists.txt",
    REPOSITORY / "Telegram" / "CMakeLists.txt",
)
RUNTIME_SUFFIXES = {
    ".c",
    ".cc",
    ".cmake",
    ".cpp",
    ".h",
    ".hpp",
    ".m",
    ".mm",
    ".qrc",
    ".strings",
    ".style",
    ".tl",
    ".txt",
    ".xml",
}
FORBIDDEN_TEXT = (
    "update.ayugram.one",
    "api.exteragram.app",
    "sentry.radolyn.com",
)
DISCLOSED_TEXT = {
    "cdn.jsdelivr.net/gh/AyuGram/Languages": {
        "Telegram/SourceFiles/ayu/ayu_lang.cpp",
    },
    "itunes.apple.com": {
        "Telegram/SourceFiles/ayu/ui/utils/itunes_search.cpp",
    },
    "ayugrambot": {
        "Telegram/SourceFiles/ayu/utils/telegram_helpers.cpp",
    },
    "exteraAuthBot": {
        "Telegram/SourceFiles/ayu/utils/telegram_helpers.cpp",
    },
}
EXPECTED_ABSENT = (
    "Telegram/SourceFiles/ayu/utils/rc_manager.cpp",
    "Telegram/SourceFiles/ayu/utils/rc_manager.h",
)


def runtime_files():
    yield from RUNTIME_FILES
    for root in RUNTIME_ROOTS:
        for path in root.rglob("*"):
            if path.is_file() and path.suffix.lower() in RUNTIME_SUFFIXES:
                yield path


def relative(path: Path) -> str:
    return path.relative_to(REPOSITORY).as_posix()


def main() -> int:
    failures = []
    forbidden = tuple((value.casefold(), value) for value in FORBIDDEN_TEXT)
    disclosed = tuple(
        (value.casefold(), value, allowed)
        for value, allowed in DISCLOSED_TEXT.items()
    )
    for path in runtime_files():
        text = path.read_text(encoding="utf-8", errors="replace")
        folded = text.casefold()
        name = relative(path)
        for needle, display in forbidden:
            if needle in folded:
                failures.append(f"{name} contains {display}")
        for needle, display, allowed in disclosed:
            if needle in folded and name not in allowed:
                failures.append(f"{name} contains undisclosed use of {display}")

    for name in EXPECTED_ABSENT:
        if (REPOSITORY / name).exists():
            failures.append(f"obsolete network source still exists: {name}")

    root_cmake = (REPOSITORY / "CMakeLists.txt").read_text(encoding="utf-8")
    forced_disable = (
        'set(DESKTOP_APP_DISABLE_AUTOUPDATE ON CACHE BOOL '
        '"Disable autoupdate." FORCE)'
    )
    if forced_disable not in root_cmake:
        failures.append("the root CMake file does not force-disable autoupdate")

    telegram_cmake = (
        REPOSITORY / "Telegram" / "CMakeLists.txt"
    ).read_text(encoding="utf-8")
    if "if (NOT DESKTOP_APP_DISABLE_AUTOUPDATE" not in telegram_cmake:
        failures.append("the updater target is not guarded by the disable option")

    crash_source = (
        REPOSITORY
        / "Telegram"
        / "SourceFiles"
        / "core"
        / "crash_report_window.cpp"
    ).read_text(encoding="utf-8")
    for symbol in ("QHttpMultiPart", "QHttpPart", "sendReport("):
        if symbol in crash_source:
            failures.append(f"crash reporting still references {symbol}")

    music_source = (
        REPOSITORY
        / "Telegram"
        / "SourceFiles"
        / "ayu"
        / "ui"
        / "components"
        / "saved_music.cpp"
    ).read_text(encoding="utf-8")
    for symbol in ("QNetwork", "http://", "https://"):
        if symbol in music_source:
            failures.append(f"saved music still references {symbol}")
    for symbol in (
        "settings.fetchMissingMusicCovers()",
        "Ayu::Ui::Itunes::FetchCover",
    ):
        if symbol not in music_source:
            failures.append(f"saved music is missing the Apple gate {symbol}")

    settings_header = (
        REPOSITORY
        / "Telegram"
        / "SourceFiles"
        / "ayu"
        / "ayu_settings.h"
    ).read_text(encoding="utf-8")
    if "_fetchMissingMusicCovers = false" not in settings_header:
        failures.append("Apple cover lookup is not default-off")

    language_source = (
        REPOSITORY
        / "Telegram"
        / "SourceFiles"
        / "ayu"
        / "ayu_lang.cpp"
    ).read_text(encoding="utf-8")
    for symbol in (
        "loadCachedLanguage()",
        "kLanguageRefreshSeconds",
        "cache.lastModified()",
        "refreshPath",
    ):
        if symbol not in language_source:
            failures.append(f"language refresh is missing {symbol}")

    helpers_source = (
        REPOSITORY
        / "Telegram"
        / "SourceFiles"
        / "ayu"
        / "utils"
        / "telegram_helpers.cpp"
    ).read_text(encoding="utf-8")
    for symbol in (
        "ayu_RegistrationDateDisclosure",
        "Ui::MakeConfirmBox",
        'u"regdate "_q',
    ):
        if symbol not in helpers_source:
            failures.append(f"registration-date disclosure is missing {symbol}")

    translator_source = (
        REPOSITORY
        / "Telegram"
        / "SourceFiles"
        / "ayu"
        / "ui"
        / "settings"
        / "settings_general.cpp"
    ).read_text(encoding="utf-8")
    for symbol in (
        "IsExternalTranslationProvider",
        "TranslationProviderConsent",
        "Ui::MakeConfirmBox",
        "SelectTranslationProvider(controller, option)",
    ):
        if symbol not in translator_source:
            failures.append(f"translation consent is missing {symbol}")

    gifs_source = (
        REPOSITORY
        / "Telegram"
        / "SourceFiles"
        / "chat_helpers"
        / "gifs_list_widget.cpp"
    ).read_text(encoding="utf-8")
    for symbol in (
        "lng_gifs_search_disclosure",
        "_gifSearchDisclosureAccepted",
        "serverConfig().gifSearchUsername",
        "Ui::MakeConfirmBox",
    ):
        if symbol not in gifs_source:
            failures.append(f"GIF search disclosure is missing {symbol}")

    location_source = (
        REPOSITORY
        / "Telegram"
        / "SourceFiles"
        / "ui"
        / "controls"
        / "location_picker.cpp"
    ).read_text(encoding="utf-8")
    for symbol in (
        "lng_maps_venues_disclosure",
        "_venuesDisclosureAccepted",
        "serverConfig().venueSearchUsername",
        "Ui::MakeConfirmBox",
    ):
        if symbol not in location_source:
            failures.append(f"venue search disclosure is missing {symbol}")

    if failures:
        for failure in failures:
            print(f"privacy check failed: {failure}", file=sys.stderr)
        return 1

    print("privacy regression check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
