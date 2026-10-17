// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/ui/boxes/link_preview_rules_box.h"

#include "ayu/ayu_settings.h"
#include "ayu/features/link_preview/link_preview_rules.h"
#include "lang/lang_keys.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/fields/input_field.h"

#include "styles/style_layers.h"
#include "styles/style_widgets.h"

namespace Ui {

void LinkPreviewRulesBox(not_null<Ui::GenericBox*> box) {
	namespace Rules = AyuFeatures::LinkPreviewRules;

	box->setTitle(tr::ayu_LinkPreviewRules());
	box->setWidth(st::boxWideWidth);

	const auto field = box->addRow(object_ptr<Ui::InputField>(
		box,
		st::defaultInputField,
		Ui::InputField::Mode::MultiLine,
		tr::ayu_LinkPreviewRulesPlaceholder(),
		Rules::Format(AyuSettings::getInstance().linkPreviewRules())));
	field->setSubmitSettings(Ui::InputField::SubmitSettings::None);
	field->setMinHeight(field->st().heightMin * 4);
	field->setMaxHeight(field->st().heightMin * 10);
	Ui::AddDividerText(
		box->verticalLayout(),
		tr::ayu_LinkPreviewRulesAbout());

	const auto save = [=] {
		const auto parsed = Rules::Parse(field->getLastText());
		if (parsed.invalidLine) {
			field->showError();
			box->showToast(tr::ayu_LinkPreviewRulesInvalidLine(
				tr::now,
				lt_line,
				QString::number(parsed.invalidLine)));
			return;
		}
		AyuSettings::getInstance().setLinkPreviewRules(parsed.rules);
		box->closeBox();
	};

	box->setFocusCallback([=] {
		field->setFocusFast();
	});
	box->addLeftButton(tr::ayu_BoxActionReset(), [=] {
		field->setText(Rules::Format(Rules::Defaults()));
	});
	box->addButton(tr::lng_settings_save(), save);
	box->addButton(tr::lng_cancel(), [=] {
		box->closeBox();
	});
}

} // namespace Ui
