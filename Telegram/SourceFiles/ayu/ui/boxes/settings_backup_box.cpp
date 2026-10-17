// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/ui/boxes/settings_backup_box.h"

#include "ayu/features/settings_backup/settings_backup.h"
#include "core/application.h"
#include "core/file_utilities.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/labels.h"
#include "window/window_session_controller.h"

#include "styles/style_layers.h"

#include <QtCore/QDir>
#include <QtCore/QFile>

namespace Ui {
namespace {

namespace Backup = AyuFeatures::SettingsBackup;

constexpr auto kMaxBackupSize = 16 * 1024 * 1024;

struct ImportState {
	Backup::Backup backup;
	Backup::Parts parts;
};

[[nodiscard]] QString BackupFilter() {
	return u"JSON (*.json);;"_q + FileDialog::AllFilesFilter();
}

void AddPart(
		not_null<Ui::GenericBox*> box,
		rpl::producer<QString> text,
		not_null<bool*> value) {
	const auto checkbox = box->addRow(
		object_ptr<Ui::Checkbox>(
			box,
			std::move(text),
			*value,
			st::defaultBoxCheckbox),
		st::boxRowPadding + QMargins(0, st::boxLittleSkip, 0, 0));
	checkbox->checkedChanges(
	) | rpl::on_next([=](bool checked) {
		*value = checked;
	}, checkbox->lifetime());
}

void AddCommonParts(
		not_null<Ui::GenericBox*> box,
		const Backup::Parts &available,
		not_null<Backup::Parts*> parts) {
	if (available.telegram) {
		AddPart(box, tr::ayu_SettingsBackupTelegram(), &parts->telegram);
	}
	if (available.experimental) {
		AddPart(
			box,
			tr::ayu_SettingsBackupExperimental(),
			&parts->experimental);
	}
	if (available.shortcuts) {
		AddPart(box, tr::ayu_SettingsBackupShortcuts(), &parts->shortcuts);
	}
	if (available.accounts) {
		AddPart(box, tr::ayu_SettingsBackupAccounts(), &parts->accounts);
	}
	if (available.ayugram) {
		AddPart(box, tr::ayu_SettingsBackupAyuGram(), &parts->ayugram);
	}
	if (available.filters) {
		AddPart(box, tr::ayu_SettingsBackupFilters(), &parts->filters);
	}
}

void ImportSettingsBox(
		not_null<Ui::GenericBox*> box,
		Backup::Backup backup) {
	box->setTitle(tr::ayu_SettingsBackupImport());
	box->setWidth(st::boxWideWidth);

	const auto state = box->lifetime().make_state<ImportState>(ImportState{
		.backup = std::move(backup),
	});
	state->parts = state->backup.parts;

	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::ayu_SettingsBackupImportAbout(
			lt_folder,
			rpl::single(QDir::toNativeSeparators(Backup::SnapshotFolder()))),
		st::boxLabel));
	AddCommonParts(box, state->backup.parts, &state->parts);
	auto skipped = QStringList();
	for (const auto &account : state->backup.accounts) {
		if (!account.loggedIn) {
			skipped.push_back(account.name);
		}
	}
	if (state->backup.parts.accounts && !skipped.isEmpty()) {
		Ui::AddDividerText(
			box->verticalLayout(),
			tr::ayu_SettingsBackupSkippedAccounts(
				lt_accounts,
				rpl::single(skipped.join(u", "_q))));
	}
	if (state->backup.parts.secrets) {
		AddPart(box, tr::ayu_SettingsBackupSecrets(), &state->parts.secrets);
	}

	box->addButton(tr::ayu_SettingsBackupImportButton(), [=] {
		if (!Backup::Apply(state->backup, state->parts)) {
			box->showToast(tr::ayu_SettingsBackupImportFailed(tr::now));
			return;
		}
		Core::Restart();
	});
	box->addButton(tr::lng_cancel(), [=] {
		box->closeBox();
	});
}

} // namespace

void ExportSettingsBox(
		not_null<Ui::GenericBox*> box,
		not_null<Window::SessionController*> controller) {
	box->setTitle(tr::ayu_SettingsBackupExport());
	box->setWidth(st::boxWideWidth);

	const auto parts = box->lifetime().make_state<Backup::Parts>(
		Backup::Parts{
			.telegram = true,
			.experimental = true,
			.shortcuts = true,
			.accounts = true,
			.ayugram = true,
			.filters = true,
		});
	auto available = *parts;
	available.secrets = true;

	AddCommonParts(box, available, parts);
	Ui::AddDividerText(
		box->verticalLayout(),
		tr::ayu_SettingsBackupExportAbout());
	AddPart(box, tr::ayu_SettingsBackupSecrets(), &parts->secrets);
	Ui::AddDividerText(
		box->verticalLayout(),
		tr::ayu_SettingsBackupSecretsAbout());

	const auto save = [=](QString &&path) {
		const auto content = Backup::Export(*parts);
		auto file = QFile(path);
		if (!file.open(QIODevice::WriteOnly)
			|| file.write(content) != content.size()) {
			box->showToast(tr::ayu_SettingsBackupWriteFailed(tr::now));
			return;
		}
		box->closeBox();
		controller->showToast(tr::ayu_SettingsBackupExported(tr::now));
	};
	box->addButton(tr::ayu_SettingsBackupExportButton(), [=] {
		FileDialog::GetWritePath(
			Core::App().getFileDialogParent(),
			tr::ayu_SettingsBackupExport(tr::now),
			BackupFilter(),
			u"ayugram-settings-%1.json"_q.arg(
				QDate::currentDate().toString(u"yyyyMMdd"_q)),
			crl::guard(box, save));
	});
	box->addButton(tr::lng_cancel(), [=] {
		box->closeBox();
	});
}

void ImportSettings(not_null<Window::SessionController*> controller) {
	const auto open = [=](FileDialog::OpenResult &&result) {
		auto content = std::move(result.remoteContent);
		if (content.isEmpty() && !result.paths.isEmpty()) {
			auto file = QFile(result.paths.front());
			if (file.size() <= kMaxBackupSize
				&& file.open(QIODevice::ReadOnly)) {
				content = file.readAll();
			}
		}
		auto backup = Backup::Parse(content);
		if (!backup) {
			controller->showToast(tr::ayu_SettingsBackupInvalid(tr::now));
			return;
		}
		controller->show(Box(ImportSettingsBox, std::move(*backup)));
	};
	FileDialog::GetOpenPath(
		Core::App().getFileDialogParent(),
		tr::ayu_SettingsBackupImport(tr::now),
		BackupFilter(),
		crl::guard(controller, open));
}

} // namespace Ui
