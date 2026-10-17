// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

#include <QtCore/QJsonObject>

namespace AyuFeatures::SettingsBackup {

struct Parts {
	bool telegram = false;
	bool experimental = false;
	bool shortcuts = false;
	bool accounts = false;
	bool ayugram = false;
	bool filters = false;
	bool secrets = false;
};

struct AccountEntry {
	uint64 userId = 0;
	QString name;
	bool loggedIn = false;
};

struct Backup {
	QJsonObject root;
	Parts parts;
	std::vector<AccountEntry> accounts;
};

[[nodiscard]] QByteArray Export(const Parts &parts);
[[nodiscard]] std::optional<Backup> Parse(const QByteArray &content);
[[nodiscard]] QString SnapshotFolder();

// Live objects are left half-updated, so callers restart right after.
[[nodiscard]] bool Apply(const Backup &backup, const Parts &parts);

} // namespace AyuFeatures::SettingsBackup
