// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#ifdef Q_OS_WIN

#include "ayu/utils/windows_utils.h"

#include "ayu/ui/ayu_logo.h"
#include "base/platform/win/base_windows_winrt.h"
#include "platform/win/windows_app_user_model_id.h"

#include <propvarutil.h>
#include <ShlObj_core.h>

bool hasIconLocation(IShellLink *shellLink, const QString &iconPath) {
	WCHAR location[MAX_PATH] = { 0 };
	auto index = 0;
	if (FAILED(shellLink->GetIconLocation(location, MAX_PATH, &index))
		|| index) {
		return false;
	}
	// The shell may keep the icon path with environment variables.
	WCHAR expanded[MAX_PATH] = { 0 };
	const auto size = ExpandEnvironmentStrings(location, expanded, MAX_PATH);
	if (!size || size > MAX_PATH) {
		return false;
	}
	const auto normalize = [](const QString &path) {
		return QDir::cleanPath(QDir::fromNativeSeparators(path));
	};
	return !normalize(QString::fromWCharArray(expanded)).compare(
		normalize(iconPath),
		Qt::CaseInsensitive);
}

bool processIcon(QString shortcut, QString iconPath) {
	if (!QFile::exists(shortcut)) {
		return false;
	}

	IShellLink *pShellLink = NULL;
	IPersistFile *pPersistFile = NULL;
	auto changed = false;

	HRESULT hr = CoCreateInstance(CLSID_ShellLink,
								  NULL,
								  CLSCTX_INPROC_SERVER,
								  IID_IShellLink,
								  (void**) &pShellLink);
	if (SUCCEEDED(hr)) {
		hr = pShellLink->QueryInterface(IID_IPersistFile, (void**) &pPersistFile);
		if (SUCCEEDED(hr)) {
			const auto shortcutPath = shortcut.toStdWString();

			if (SUCCEEDED(pPersistFile->Load(shortcutPath.c_str(), STGM_READWRITE))
				&& !hasIconLocation(pShellLink, iconPath)) {
				pShellLink->SetIconLocation(iconPath.toStdWString().c_str(), 0);
				changed = SUCCEEDED(pPersistFile->Save(shortcutPath.c_str(), TRUE));
			}

			pPersistFile->Release();
		}

		pShellLink->Release();
	}
	return changed;
}

bool processLegacy(const QString &iconPath) {
	const auto appdata = QDir::fromNativeSeparators(qgetenv("APPDATA"));
	auto shortcut = appdata + "/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar/AyuGram Desktop.lnk";
	if (!QFile::exists(shortcut)) {
		shortcut = appdata + "/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar/AyuGram.lnk";
	}
	if (!QFile::exists(shortcut)) {
		return false;
	}

	return processIcon(shortcut, iconPath);
}

bool processNewPinned(const QString &iconPath) {
	if (!SUCCEEDED(CoInitialize(0))) {
		return false;
	}
	const auto coGuard = gsl::finally([]
	{
		CoUninitialize();
	});

	const auto path = Platform::AppUserModelId::PinnedIconsPath();
	const auto native = QDir::toNativeSeparators(path).toStdWString();

	const auto srcid = Platform::AppUserModelId::MyExecutablePathId();
	if (!srcid) {
		return false;
	}

	WIN32_FIND_DATA findData;
	HANDLE findHandle = FindFirstFileEx(
		(native + L"*").c_str(),
		FindExInfoStandard,
		&findData,
		FindExSearchNameMatch,
		0,
		0);
	if (findHandle == INVALID_HANDLE_VALUE) {
		return false;
	}

	auto changed = false;
	do {
		std::wstring fname = native + findData.cFileName;
		const auto filePath = QString::fromStdWString(fname);
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			continue;
		}

		DWORD attributes = GetFileAttributes(fname.c_str());
		if (attributes >= 0xFFFFFFF) {
			continue; // file does not exist
		}

		auto shellLink = base::WinRT::TryCreateInstance<IShellLink>(
			CLSID_ShellLink);
		if (!shellLink) {
			continue;
		}

		auto persistFile = shellLink.try_as<IPersistFile>();
		if (!persistFile) {
			continue;
		}

		auto hr = persistFile->Load(fname.c_str(), STGM_READWRITE);
		if (!SUCCEEDED(hr)) continue;

		WCHAR dst[MAX_PATH] = {0};
		hr = shellLink->GetPath(dst, MAX_PATH, nullptr, 0);
		if (!SUCCEEDED(hr)) continue;

		if (Platform::AppUserModelId::GetUniqueFileId(dst) == srcid) {
			auto propertyStore = shellLink.try_as<IPropertyStore>();
			if (!propertyStore) {
				continue;
			}

			changed |= processIcon(filePath, iconPath);
		}
	} while (FindNextFile(findHandle, &findData));
	DWORD errorCode = GetLastError();
	if (errorCode && errorCode != ERROR_NO_MORE_FILES) {
		return changed;
	}
	FindClose(findHandle);
	return changed;
}

bool processNewShortcuts(const QString &iconPath) {
	const auto path = Platform::AppUserModelId::systemShortcutPath();
	if (path.isEmpty()) {
		return false;
	}

	const auto shortcuts = {
		path + u"AyuGram Desktop/AyuGram.lnk"_q,
		path + u"AyuGram/AyuGram.lnk"_q,
		path + u"AyuGram.lnk"_q,
	};
	auto changed = false;
	for (const auto &shortcut : shortcuts) {
		const auto native = QDir::toNativeSeparators(shortcut).toStdWString();

		DWORD attributes = GetFileAttributes(native.c_str());
		if (attributes >= 0xFFFFFFF) {
			continue;
		}

		changed |= processIcon(QString::fromStdWString(native), iconPath);
	}
	return changed;
}

void reloadAppIconFromTaskBar() {
	auto changed = AyuAssets::loadAppIco();
	const auto iconPath = AyuAssets::appIcoPath();

	changed |= processNewPinned(iconPath);
	changed |= processNewShortcuts(iconPath);
	changed |= processLegacy(iconPath);

	// Flushes every icon in the shell, so a launch with nothing to update skips it.
	if (changed) {
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
	}
}

#endif
