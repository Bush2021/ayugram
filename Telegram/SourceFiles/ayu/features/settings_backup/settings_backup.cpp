// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/features/settings_backup/settings_backup.h"

#include "ayu/ayu_settings.h"
#include "ayu/features/filters/filters_utils.h"
#include "base/options.h"
#include "base/parse_helper.h"
#include "base/unixtime.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/version.h"
#include "data/data_user.h"
#include "main/main_account.h"
#include "main/main_domain.h"
#include "main/main_session.h"
#include "main/main_session_settings.h"
#include "settings.h"
#include "storage/localstorage.h"
#include "ui/power_saving.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>

namespace AyuFeatures::SettingsBackup {
namespace {

constexpr auto kVersion = 1;
constexpr auto kSnapshotsKept = 5;

const auto kFormat = u"ayugram-settings-backup"_q;
const auto kTelegram = u"telegram"_q;
const auto kExperimental = u"experimental"_q;
const auto kShortcuts = u"shortcuts"_q;
const auto kAccounts = u"accounts"_q;
const auto kAyuGram = u"ayugram"_q;
const auto kFilters = u"filters"_q;
const auto kSecrets = u"secrets"_q;

struct Preserved {
	Core::WindowPosition window;
	Core::WindowPosition mediaView;
	Core::WindowPosition iv;
	Core::WindowPosition callPanel;
	QByteArray videoPip;
	QString playbackDevice;
	QString captureDevice;
	QString cameraDevice;
	QString callPlaybackDevice;
	QString callCaptureDevice;
	QString downloadPath;
	QByteArray downloadPathBookmark;
	std::vector<uint64> accountsOrder;
	std::optional<uint64> macRoundIconDigest;
	QString customDeviceModel;
	QByteArray tonsiteToken;
	QByteArray proxy;
	QString apiKey;
	bool systemUnlock = false;
};

[[nodiscard]] QString ShortcutsPath() {
	return cWorkingDir() + u"tdata/shortcuts-custom.json"_q;
}

[[nodiscard]] QString Encode(const QByteArray &data) {
	return QString::fromLatin1(data.toBase64());
}

[[nodiscard]] std::optional<QByteArray> Decode(const QJsonValue &value) {
	if (!value.isString()) {
		return std::nullopt;
	}
	auto result = QByteArray::fromBase64Encoding(
		value.toString().toLatin1(),
		QByteArray::AbortOnBase64DecodingErrors);
	if (!result || result.decoded.isEmpty()) {
		return std::nullopt;
	}
	return std::move(result.decoded);
}

[[nodiscard]] QString ToCompactJson(const QJsonObject &object) {
	return QString::fromUtf8(
		QJsonDocument(object).toJson(QJsonDocument::Compact));
}

[[nodiscard]] Main::Session *FindSession(uint64 userId) {
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (account->sessionExists()
			&& account->session().userId().bare == userId) {
			return &account->session();
		}
	}
	return nullptr;
}

void WritePrefString(
		Core::Settings &settings,
		std::string_view key,
		const QString &value) {
	if (value.isEmpty()) {
		settings.clearPref(key);
	} else {
		settings.writePref<QString>(key, value);
	}
}

// Secrets live in their own section, so the main block never carries them.
[[nodiscard]] QByteArray SerializeCoreSettings() {
	auto &settings = Core::App().settings();
	const auto key = OpenAiTranslationSettings::ApiKeyPref();
	const auto apiKey = settings.readPref<QString>(key);
	const auto proxy = settings.proxy().serialize();
	const auto tonsite = settings.tonsiteStorageToken();
	settings.clearPref(key);
	settings.proxy().setFromSerialized(Core::SettingsProxy().serialize());
	settings.setTonsiteStorageToken(QByteArray());
	auto result = settings.serialize();
	settings.setTonsiteStorageToken(tonsite);
	settings.proxy().setFromSerialized(proxy);
	WritePrefString(settings, key, apiKey);
	return result;
}

[[nodiscard]] QJsonArray ReadShortcuts() {
	auto file = QFile(ShortcutsPath());
	if (!file.open(QIODevice::ReadOnly)) {
		return QJsonArray();
	}
	const auto document = QJsonDocument::fromJson(
		base::parse::stripComments(file.readAll()));
	return document.isArray() ? document.array() : QJsonArray();
}

[[nodiscard]] QJsonObject SerializeAccounts() {
	auto result = QJsonObject();
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (!account->sessionExists()) {
			continue;
		}
		const auto session = &account->session();
		result.insert(
			QString::number(session->userId().bare),
			QJsonObject{
				{ u"name"_q, session->user()->name() },
				{ u"session"_q, Encode(session->settings().serialize()) },
			});
	}
	return result;
}

[[nodiscard]] QJsonObject SerializeAyuGram() {
	auto json = nlohmann::json(AyuSettings::getInstance());
	auto &openAi = json["openAiTranslationSettings"];
	if (openAi.is_object()) {
		openAi.erase("apiKey");
	}
	return QJsonDocument::fromJson(
		QByteArray::fromStdString(json.dump())).object();
}

[[nodiscard]] QJsonObject SerializeSecrets() {
	const auto &openAi = AyuSettings::getInstance().openAiTranslationSettings();
	return QJsonObject{
		{ u"openAiApiKey"_q, openAi.apiKey() },
		{ u"proxy"_q, Encode(Core::App().settings().proxy().serialize()) },
	};
}

[[nodiscard]] Preserved Preserve(Core::Settings &settings) {
	return {
		.window = settings.windowPosition(),
		.mediaView = settings.mediaViewPosition(),
		.iv = settings.ivPosition(),
		.callPanel = settings.callPanelPosition(),
		.videoPip = settings.videoPipGeometry(),
		.playbackDevice = settings.playbackDeviceId(),
		.captureDevice = settings.captureDeviceId(),
		.cameraDevice = settings.cameraDeviceId(),
		.callPlaybackDevice = settings.callPlaybackDeviceId(),
		.callCaptureDevice = settings.callCaptureDeviceId(),
		.downloadPath = settings.downloadPath(),
		.downloadPathBookmark = settings.downloadPathBookmark(),
		.accountsOrder = settings.accountsOrder(),
		.macRoundIconDigest = settings.macRoundIconDigest(),
		.customDeviceModel = settings.customDeviceModel(),
		.tonsiteToken = settings.tonsiteStorageToken(),
		.proxy = settings.proxy().serialize(),
		.apiKey = settings.readPref<QString>(
			OpenAiTranslationSettings::ApiKeyPref()),
		.systemUnlock = settings.systemUnlockEnabled(),
	};
}

void Restore(Core::Settings &settings, const Preserved &was) {
	settings.setWindowPosition(was.window);
	settings.setMediaViewPosition(was.mediaView);
	settings.setIvPosition(was.iv);
	settings.setCallPanelPosition(was.callPanel);
	settings.setVideoPipGeometry(was.videoPip);
	settings.setPlaybackDeviceId(was.playbackDevice);
	settings.setCaptureDeviceId(was.captureDevice);
	settings.setCameraDeviceId(was.cameraDevice);
	settings.setCallPlaybackDeviceId(was.callPlaybackDevice);
	settings.setCallCaptureDeviceId(was.callCaptureDevice);
	settings.setDownloadPath(was.downloadPath);
	settings.setDownloadPathBookmark(was.downloadPathBookmark);
	settings.setAccountsOrder(was.accountsOrder);
	settings.setMacRoundIconDigest(was.macRoundIconDigest);
	settings.setCustomDeviceModel(was.customDeviceModel);
	settings.setTonsiteStorageToken(was.tonsiteToken);
	settings.setSystemUnlockEnabled(was.systemUnlock);
	settings.proxy().setFromSerialized(was.proxy);
	WritePrefString(
		settings,
		OpenAiTranslationSettings::ApiKeyPref(),
		was.apiKey);
}

[[nodiscard]] bool ApplyAyuGram(const QJsonObject &object) {
	auto &settings = AyuSettings::getInstance();
	try {
		const auto json = nlohmann::json::parse(
			QJsonDocument(object).toJson(
				QJsonDocument::Compact).toStdString());
		::from_json(json, settings);
	} catch (...) {
		AyuSettings::load();
		return false;
	}
	settings.validate();
	AyuSettings::save();
	return true;
}

void ApplyTelegram(const QJsonObject &object) {
	auto &settings = Core::App().settings();
	const auto was = Preserve(settings);
	if (const auto core = Decode(object.value(u"core"_q))) {
		settings.addFromSerialized(*core);
	}
	Restore(settings, was);
	PowerSaving::Set(PowerSaving::Flags::from_raw(
		object.value(u"powerSaving"_q).toInt()));
	cSetStartMinimized(object.value(u"startMinimized"_q).toBool());
}

void ApplySecrets(const QJsonObject &object) {
	auto &settings = Core::App().settings();
	if (const auto proxy = Decode(object.value(u"proxy"_q))) {
		settings.proxy().setFromSerialized(*proxy);
	}
	WritePrefString(
		settings,
		OpenAiTranslationSettings::ApiKeyPref(),
		object.value(u"openAiApiKey"_q).toString());
}

void ApplyShortcuts(const QJsonArray &shortcuts) {
	auto file = QFile(ShortcutsPath());
	if (!file.open(QIODevice::WriteOnly)) {
		LOG(("Settings Backup Error: Could not write '%1'."
			).arg(file.fileName()));
		return;
	}
	file.write(QJsonDocument(shortcuts).toJson(QJsonDocument::Indented));
}

void ApplyAccounts(const QJsonObject &accounts) {
	for (const auto &[index, account] : Core::App().domain().accounts()) {
		if (!account->sessionExists()) {
			continue;
		}
		const auto session = &account->session();
		const auto entry = accounts.value(
			QString::number(session->userId().bare)).toObject();
		const auto serialized = Decode(entry.value(u"session"_q));
		if (!serialized) {
			continue;
		}
		auto &settings = session->settings();
		auto unreviewed = settings.unreviewed();
		const auto setupEmail = settings.setupEmailState();
		settings.addFromSerialized(*serialized);
		settings.setUnreviewed(std::move(unreviewed));
		settings.setSetupEmailState(setupEmail);
		session->saveSettings();
	}
}

void ApplyFilters(const QJsonObject &filters) {
	auto &utils = FilterUtils::getInstance();
	utils.applyChanges(utils.prepareChanges(filters));
}

[[nodiscard]] bool SaveSnapshot() {
	const auto folder = SnapshotFolder();
	if (!QDir().mkpath(folder)) {
		return false;
	}
	const auto content = Export({
		.telegram = true,
		.experimental = true,
		.shortcuts = true,
		.accounts = true,
		.ayugram = true,
		.filters = true,
		.secrets = true,
	});
	auto file = QFile(folder + u"settings-%1.json"_q.arg(
		QDateTime::currentDateTime().toString(u"yyyyMMdd-HHmmss"_q)));
	if (!file.open(QIODevice::WriteOnly)
		|| file.write(content) != content.size()) {
		return false;
	}
	file.close();
	auto snapshots = QDir(folder).entryList(
		{ u"settings-*.json"_q },
		QDir::Files,
		QDir::Name);
	while (snapshots.size() > kSnapshotsKept) {
		QFile::remove(folder + snapshots.takeFirst());
	}
	return true;
}

} // namespace

QByteArray Export(const Parts &parts) {
	auto root = QJsonObject();
	root.insert(u"format"_q, kFormat);
	root.insert(u"version"_q, kVersion);
	root.insert(u"createdAt"_q, base::unixtime::now());
	root.insert(u"appVersion"_q, QString::fromLatin1(AppVersionStr));
	if (parts.telegram) {
		root.insert(kTelegram, QJsonObject{
			{ u"core"_q, Encode(SerializeCoreSettings()) },
			{ u"powerSaving"_q, int(PowerSaving::Current().value()) },
			{ u"startMinimized"_q, cStartMinimized() },
		});
	}
	if (parts.experimental) {
		root.insert(
			kExperimental,
			QJsonDocument::fromJson(
				base::options::serialize().toUtf8()).object());
	}
	if (parts.shortcuts) {
		root.insert(kShortcuts, ReadShortcuts());
	}
	if (parts.accounts) {
		root.insert(kAccounts, SerializeAccounts());
	}
	if (parts.ayugram) {
		root.insert(kAyuGram, SerializeAyuGram());
	}
	if (parts.filters) {
		root.insert(
			kFilters,
			QJsonDocument::fromJson(
				FilterUtils::exportFilters().toUtf8()).object());
	}
	if (parts.secrets) {
		root.insert(kSecrets, SerializeSecrets());
	}
	return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::optional<Backup> Parse(const QByteArray &content) {
	auto error = QJsonParseError();
	const auto document = QJsonDocument::fromJson(content, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return std::nullopt;
	}
	auto result = Backup{ .root = document.object() };
	const auto &root = result.root;
	if (root.value(u"format"_q).toString() != kFormat
		|| root.value(u"version"_q).toInt() != kVersion) {
		return std::nullopt;
	}
	auto &parts = result.parts;
	if (root.contains(kTelegram)) {
		const auto telegram = root.value(kTelegram).toObject();
		if (!Decode(telegram.value(u"core"_q))) {
			return std::nullopt;
		}
		parts.telegram = true;
	}
	if (root.contains(kExperimental)) {
		if (!root.value(kExperimental).isObject()) {
			return std::nullopt;
		}
		parts.experimental = true;
	}
	if (root.contains(kShortcuts)) {
		if (!root.value(kShortcuts).isArray()) {
			return std::nullopt;
		}
		parts.shortcuts = true;
	}
	if (root.contains(kAccounts)) {
		const auto value = root.value(kAccounts);
		if (!value.isObject()) {
			return std::nullopt;
		}
		const auto accounts = value.toObject();
		for (auto i = accounts.constBegin(); i != accounts.constEnd(); ++i) {
			auto valid = false;
			const auto userId = i.key().toULongLong(&valid);
			const auto entry = i.value().toObject();
			if (!valid || !Decode(entry.value(u"session"_q))) {
				return std::nullopt;
			}
			result.accounts.push_back({
				.userId = userId,
				.name = entry.value(u"name"_q).toString(),
				.loggedIn = (FindSession(userId) != nullptr),
			});
		}
		parts.accounts = !result.accounts.empty();
	}
	if (root.contains(kAyuGram)) {
		if (!root.value(kAyuGram).isObject()) {
			return std::nullopt;
		}
		parts.ayugram = true;
	}
	if (root.contains(kFilters)) {
		if (!root.value(kFilters).isObject()) {
			return std::nullopt;
		}
		parts.filters = true;
	}
	if (root.contains(kSecrets)) {
		const auto secrets = root.value(kSecrets).toObject();
		if (!secrets.value(u"openAiApiKey"_q).isString()
			|| !Decode(secrets.value(u"proxy"_q))) {
			return std::nullopt;
		}
		parts.secrets = true;
	}
	return result;
}

QString SnapshotFolder() {
	return cWorkingDir() + u"tdata/ayu_backups/"_q;
}

bool Apply(const Backup &backup, const Parts &parts) {
	const auto &root = backup.root;
	const auto use = [&](bool Parts::*part) {
		return (parts.*part) && (backup.parts.*part);
	};
	if (!SaveSnapshot()) {
		return false;
	}
	const auto experimental = base::options::serialize();
	if (use(&Parts::experimental)
		&& !base::options::deserialize(
			ToCompactJson(root.value(kExperimental).toObject()))) {
		return false;
	}
	if (use(&Parts::ayugram)
		&& !ApplyAyuGram(root.value(kAyuGram).toObject())) {
		if (!base::options::deserialize(experimental)) {
			LOG(("Settings Backup Error: "
				"Could not restore experimental options."));
		}
		return false;
	}
	if (use(&Parts::telegram)) {
		ApplyTelegram(root.value(kTelegram).toObject());
	}
	if (use(&Parts::secrets)) {
		ApplySecrets(root.value(kSecrets).toObject());
	}
	if (use(&Parts::shortcuts)) {
		ApplyShortcuts(root.value(kShortcuts).toArray());
	}
	if (use(&Parts::accounts)) {
		ApplyAccounts(root.value(kAccounts).toObject());
	}
	if (use(&Parts::filters)) {
		ApplyFilters(root.value(kFilters).toObject());
	}
	Local::writeSettings();
	return true;
}

} // namespace AyuFeatures::SettingsBackup
