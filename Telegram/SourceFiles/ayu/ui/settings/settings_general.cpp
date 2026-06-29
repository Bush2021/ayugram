// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#include "ayu/ui/settings/settings_general.h"

#include "base/platform/base_platform_info.h"
#include "ayu/ui/settings/ayu_builder.h"
#include "ayu/ui/settings/settings_ayu_utils.h"
#include "ayu/ui/settings/settings_main.h"
#include "ayu/ayu_settings.h"
#include "core/application.h"
#include "lang/lang_text_entity.h"
#include "lang_auto.h"
#include "platform/platform_translate_provider.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "ui/layers/generic_box.h"
#include "ui/rp_widget.h"
#include "ui/boxes/single_choice_box.h"
#include "ui/toast/toast.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/fields/password_input.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_controller.h"
#include "window/window_session_controller.h"

#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"

#include <memory>

namespace Settings {

using namespace Builder;
using namespace AyuBuilder;

namespace {

[[nodiscard]] bool IsOpenAiProvider(TranslationProvider provider) {
	return (provider == TranslationProvider::OpenAI);
}

void PrepareMultilineInput(not_null<Ui::InputField*> field) {
	field->setMinHeight(field->st().heightMin * 4);
	field->setMaxHeight(field->st().heightMin * 8);
}

void EditOpenAiTranslationSettings(
		not_null<Window::SessionController*> controller) {
	const auto settings = &AyuSettings::getInstance();
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		const auto &current = settings->openAiTranslationSettings();
		box->setTitle(tr::ayu_OpenAiTranslationSettings());
		box->setWidth(st::boxWideWidth);

		const auto model = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			tr::ayu_OpenAiTranslationModel(),
			current.model()));
		const auto apiBaseOrEndpoint = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			tr::ayu_OpenAiTranslationApiUrl(),
			current.apiBaseOrEndpoint()));
		Ui::AddDividerText(
			box->verticalLayout(),
			tr::ayu_OpenAiTranslationApiUrlAbout());
		const auto apiKeyWrap = box->addRow(object_ptr<Ui::RpWidget>(box));
		const auto apiKey = Ui::CreateChild<Ui::PasswordInput>(
			apiKeyWrap,
			st::defaultInputField,
			tr::ayu_OpenAiTranslationApiKey(),
			current.apiKey());
		const auto apiKeyVisible = std::make_shared<bool>(false);
		const auto apiKeyToggle = Ui::CreateChild<Ui::LinkButton>(
			apiKeyWrap,
			tr::lng_usernames_activate_confirm(tr::now));
		const auto updateApiKeyLayout = [=] {
			const auto width = apiKeyWrap->width();
			const auto height = (apiKey->height() > apiKeyToggle->height())
				? apiKey->height()
				: apiKeyToggle->height();
			const auto fieldWidth = (width > apiKeyToggle->width())
				? (width - apiKeyToggle->width())
				: 0;
			apiKeyWrap->resize(width, height);
			apiKey->resize(fieldWidth, apiKey->height());
			apiKey->move(0, (height - apiKey->height()) / 2);
			apiKeyToggle->moveToRight(0, (height - apiKeyToggle->height()) / 2);
		};
		const auto updateApiKeyVisibility = [=] {
			apiKey->setEchoMode(*apiKeyVisible
				? QLineEdit::Normal
				: QLineEdit::Password);
			apiKeyToggle->setText(*apiKeyVisible
				? tr::lng_usernames_deactivate_confirm(tr::now)
				: tr::lng_usernames_activate_confirm(tr::now));
		};
		updateApiKeyVisibility();
		apiKeyToggle->setClickedCallback([=] {
			*apiKeyVisible = !*apiKeyVisible;
			updateApiKeyVisibility();
		});
		apiKey->heightValue(
		) | rpl::on_next([=](int) {
			updateApiKeyLayout();
		}, apiKey->lifetime());
		apiKeyWrap->widthValue(
		) | rpl::on_next([=](int) {
			updateApiKeyLayout();
		}, apiKey->lifetime());
		apiKeyToggle->widthValue(
		) | rpl::on_next([=](int) {
			updateApiKeyLayout();
		}, apiKey->lifetime());
		updateApiKeyLayout();

		const auto systemPrompt = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			Ui::InputField::Mode::MultiLine,
			tr::ayu_OpenAiTranslationSystemPrompt(),
			current.systemPrompt()));
		PrepareMultilineInput(systemPrompt);

		const auto promptTemplate = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			Ui::InputField::Mode::MultiLine,
			tr::ayu_OpenAiTranslationPrompt(),
			current.promptTemplate()));
		PrepareMultilineInput(promptTemplate);

		Ui::AddSkip(box->verticalLayout());
		Ui::AddDividerText(
			box->verticalLayout(),
			rpl::single(tr::ayu_OpenAiTranslationAbout(
				tr::now,
				lt_to,
				QStringLiteral("{to}"),
				lt_message_count,
				QStringLiteral("{message_count}"),
				lt_messages_json,
				QStringLiteral("{messages_json}"))));

		const auto reset = [=] {
			model->setText(OpenAiTranslationSettings::DefaultModel());
			apiBaseOrEndpoint->setText(
				OpenAiTranslationSettings::DefaultApiBaseOrEndpoint());
			apiKey->setText(QString());
			*apiKeyVisible = false;
			updateApiKeyVisibility();
			systemPrompt->setText(
				OpenAiTranslationSettings::DefaultSystemPrompt());
			promptTemplate->setText(
				OpenAiTranslationSettings::DefaultPromptTemplate());
		};
		const auto save = [=] {
			settings->openAiTranslationSettings().apply(
				model->getLastText(),
				apiBaseOrEndpoint->getLastText(),
				apiKey->getLastText(),
				systemPrompt->getLastText(),
				promptTemplate->getLastText());
			box->closeBox();
		};

		box->setFocusCallback([=] {
			model->setFocusFast();
		});
		box->addLeftButton(tr::ayu_BoxActionReset(), reset);
		box->addButton(tr::lng_settings_save(), save);
		box->addButton(tr::lng_cancel(), [=] {
			box->closeBox();
		});
	}));
}

void BuildTranslator(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	builder.addSubsectionTitle(tr::lng_translate_settings_subtitle());

	auto *settings = &AyuSettings::getInstance();

	const auto options = std::vector{
		std::pair(TranslationProvider::Telegram, QString("Telegram")),
		std::pair(TranslationProvider::Google, QString("Google")),
		std::pair(TranslationProvider::Yandex, QString("Yandex")),
		std::pair(TranslationProvider::OpenAI, QString("OpenAI")),
	};
	const auto nativeAvailable = Platform::IsTranslateProviderAvailable();
	auto availableOptions = options;
	if (nativeAvailable) {
		availableOptions.push_back(std::pair(
			TranslationProvider::Native,
			[] {
				if constexpr (Platform::IsMac()) {
					return QString("macOS");
				} else if constexpr (Platform::IsWindows()) {
					return QString("Windows");
				} else {
					return QString("Linux");
				}
			}()));
	}
	auto optionLabels = std::vector<QString>();
	optionLabels.reserve(availableOptions.size());
	for (const auto &option : availableOptions) {
		optionLabels.push_back(option.second);
	}

	const auto getIndex = [=](TranslationProvider val) {
		const auto i = ranges::find(
			availableOptions,
			val,
			&std::pair<TranslationProvider, QString>::first);
		return (i != end(availableOptions))
			? int(i - begin(availableOptions))
			: 0;
	};

	auto currentVal = AyuSettings::getInstance().translationProviderValue()
		| rpl::map(getIndex)
		| rpl::map([=](int val) { return availableOptions[val].second; });

	const auto button = builder.addButton({
		.id = u"ayu/translationProvider"_q,
		.title = tr::ayu_TranslationProvider(),
		.st = &st::settingsButtonNoIcon,
		.label = std::move(currentVal),
		.onClick = [=] {
			if (const auto controller = Core::App().activeWindow()->sessionController()) {
				controller->show(Box(
						[=](not_null<Ui::GenericBox*> box) {
							const auto save = [=](int index) {
								const auto option = availableOptions[index].first;
								AyuSettings::getInstance().setTranslationProvider(option);

								if constexpr (Platform::IsMac()) {
									if (option == TranslationProvider::Native) {
										controller->showToast(Ui::Toast::Config{
											.text = tr::lng_translate_settings_use_platform_mac_about(tr::now, tr::rich),
											.duration = 6 * crl::time(1000)
										});
									}
								}
							};
							SingleChoiceBox(box, {
								.title = tr::ayu_TranslationProvider(),
								.options = optionLabels,
								.initialSelection = getIndex(settings->translationProvider()),
								.callback = save,
							});
						}));
			}
		},
	});
	if (button) {
		ayu.addBetaBadge(button);
	}

	builder.addButton({
		.id = u"ayu/openaiTranslationSettings"_q,
		.title = tr::ayu_OpenAiTranslationSettings(),
		.st = &st::settingsButtonNoIcon,
		.label = settings->openAiTranslationSettings().modelValue()
			| rpl::map([](const QString &model) {
				const auto trimmed = model.trimmed();
				return trimmed.isEmpty()
					? OpenAiTranslationSettings::DefaultModel()
					: trimmed;
			}),
		.onClick = [=] {
			if (const auto controller = Core::App().activeWindow()->sessionController()) {
				EditOpenAiTranslationSettings(controller);
			}
		},
		.shown = settings->translationProviderValue()
			| rpl::map([](TranslationProvider provider) {
				return IsOpenAiProvider(provider);
			}),
	});
}

void BuildShowPeerId(SectionBuilder &builder) {
	auto *settings = &AyuSettings::getInstance();

	const auto options = std::vector{
		QString(tr::ayu_SettingsShowID_Hide(tr::now)),
		QString("Telegram API"),
		QString("Bot API")
	};

	auto currentVal = AyuSettings::getInstance().showPeerIdValue()
		| rpl::map([=](PeerIdDisplay val) {
			return options[static_cast<int>(val)];
		});

	const auto controller = builder.controller();
	builder.addButton({
		.id = u"ayu/showPeerId"_q,
		.altIds = { u"ayu/showIdAndDc"_q },
		.title = tr::ayu_SettingsShowID(),
		.st = &st::settingsButtonNoIcon,
		.label = std::move(currentVal),
		.onClick = [=] {
			controller->show(Box(
				[=](not_null<Ui::GenericBox*> box) {
					const auto save = [=](int index) {
						AyuSettings::getInstance().setShowPeerId(
							static_cast<PeerIdDisplay>(index));
					};
					SingleChoiceBox(box, {
						.title = tr::ayu_SettingsShowID(),
						.options = options,
						.initialSelection = static_cast<int>(settings->showPeerId()),
						.callback = save,
					});
				}));
		},
	});
}

void BuildQoLToggles(SectionBuilder &builder, AyuSectionBuilder &ayu) {
	auto *settings = &AyuSettings::getInstance();

	BuildTranslator(builder, ayu);
	ayu.addSectionDivider();

	builder.addSubsectionTitle(tr::ayu_CategoryGeneral());

	const auto controller = builder.controller();
	ayu.addToggle({
		.id = u"ayu/disableStories"_q,
		.altIds = { u"ayu/hideStories"_q },
		.title = tr::ayu_DisableStories(),
		.getter = [=] { return settings->disableStories(); },
		.setter = [=](bool enabled) {
			AyuSettings::getInstance().setDisableStories(enabled);
			ShowRestartPrompt(controller);
		},
	});

	ayu.addSettingToggle({
		.id = u"ayu/disableOpenLinkWarning"_q,
		.title = tr::ayu_DisableOpenLinkWarning(),
		.getter = &AyuSettings::disableOpenLinkWarning,
		.setter = &AyuSettings::setDisableOpenLinkWarning,
	});

	ayu.addCollapsibleToggle({
		.id = u"ayu/similarChannels"_q,
		.title = tr::ayu_DisableSimilarChannels(),
		.checkboxes = {
			NestedEntry{
				tr::ayu_CollapseSimilarChannels(tr::now),
				[] { return AyuSettings::getInstance().collapseSimilarChannels(); },
				[](bool v) { AyuSettings::getInstance().setCollapseSimilarChannels(v); }
			},
			NestedEntry{
				tr::ayu_HideSimilarChannelsTab(tr::now),
				[] { return AyuSettings::getInstance().hideSimilarChannels(); },
				[](bool v) { AyuSettings::getInstance().setHideSimilarChannels(v); }
			}
		},
		.toggledWhenAll = true,
	});

	ayu.addSettingToggle({
		.id = u"ayu/disablePullToNextChannel"_q,
		.title = tr::ayu_DisablePullToNextChannel(),
		.getter = &AyuSettings::disablePullToNextChannel,
		.setter = &AyuSettings::setDisablePullToNextChannel,
	});

	ayu.addSettingToggle({
		.id = u"ayu/disableNotificationsDelay"_q,
		.title = tr::ayu_DisableNotificationsDelay(),
		.getter = &AyuSettings::disableNotificationsDelay,
		.setter = &AyuSettings::setDisableNotificationsDelay,
	});

	ayu.addSectionDivider();

	const auto zalgoButton = builder.addButton({
		.id = u"ayu/filterZalgo"_q,
		.title = tr::ayu_FilterZalgo(),
		.st = &st::settingsButtonNoIcon,
		.toggled = rpl::single(settings->filterZalgo()),
	});
	if (zalgoButton) {
		zalgoButton->toggledValue(
		) | rpl::filter(
			[=](bool enabled) {
				return (enabled != settings->filterZalgo());
			}
		) | on_next(
			[=](bool enabled) {
				AyuSettings::getInstance().setFilterZalgo(enabled);
				ShowRestartPrompt(controller);
			},
			zalgoButton->lifetime());
		ayu.addBetaBadge(zalgoButton);
	}

	ayu.addSettingToggle({
		.id = u"ayu/improveLinkPreviews"_q,
		.title = tr::ayu_ImproveLinkPreviews(),
		.getter = &AyuSettings::improveLinkPreviews,
		.setter = &AyuSettings::setImproveLinkPreviews,
	});
	ayu.addSettingToggle({
		.id = u"ayu/showMessageSeconds"_q,
		.altIds = { u"ayu/formatTimeWithSeconds"_q },
		.title = tr::ayu_SettingsShowMessageSeconds(),
		.getter = &AyuSettings::showMessageSeconds,
		.setter = &AyuSettings::setShowMessageSeconds,
	});

	BuildShowPeerId(builder);

	ayu.addSectionDivider();

	builder.addSubsectionTitle(rpl::single(QString("Webview")));

	ayu.addSettingToggle({
		.id = u"ayu/spoofWebviewAsAndroid"_q,
		.title = tr::ayu_SettingsSpoofWebviewAsAndroid(),
		.getter = &AyuSettings::spoofWebviewAsAndroid,
		.setter = &AyuSettings::setSpoofWebviewAsAndroid,
	});

	ayu.addCollapsibleToggle({
		.id = u"ayu/biggerWindow"_q,
		.title = tr::ayu_SettingsBiggerWindow(),
		.checkboxes = {
			NestedEntry{
				tr::ayu_SettingsIncreaseWebviewHeight(tr::now),
				[] { return AyuSettings::getInstance().increaseWebviewHeight(); },
				[](bool v) { AyuSettings::getInstance().setIncreaseWebviewHeight(v); }
			},
			NestedEntry{
				tr::ayu_SettingsIncreaseWebviewWidth(tr::now),
				[] { return AyuSettings::getInstance().increaseWebviewWidth(); },
				[](bool v) { AyuSettings::getInstance().setIncreaseWebviewWidth(v); }
			}
		},
		.toggledWhenAll = false,
	});

	ayu.addSectionDivider();

	builder.addSubsectionTitle(tr::ayu_ConfirmationsTitle());

	ayu.addSettingToggle({
		.id = u"ayu/stickerConfirmation"_q,
		.title = tr::ayu_StickerConfirmation(),
		.getter = &AyuSettings::stickerConfirmation,
		.setter = &AyuSettings::setStickerConfirmation,
	});
	ayu.addSettingToggle({
		.id = u"ayu/gifConfirmation"_q,
		.title = tr::ayu_GIFConfirmation(),
		.getter = &AyuSettings::gifConfirmation,
		.setter = &AyuSettings::setGifConfirmation,
	});
	ayu.addSettingToggle({
		.id = u"ayu/voiceConfirmation"_q,
		.title = tr::ayu_VoiceConfirmation(),
		.getter = &AyuSettings::voiceConfirmation,
		.setter = &AyuSettings::setVoiceConfirmation,
	});
}

const auto kMeta = BuildHelper({
	.id = AyuGeneral::Id(),
	.parentId = AyuMain::Id(),
	.title = &tr::ayu_CategoryGeneral,
	.icon = &st::menuIconShowAll,
}, [](SectionBuilder &builder) {
	auto ayu = AyuSectionBuilder(builder);

	builder.addSkip();
	BuildQoLToggles(builder, ayu);
	builder.addSkip();
});

} // namespace

rpl::producer<QString> AyuGeneral::title() {
	return tr::ayu_CategoryGeneral();
}

AyuGeneral::AyuGeneral(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

void AyuGeneral::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMeta.build);
	Ui::ResizeFitChild(this, content);
}

Type AyuGeneralId() {
	return AyuGeneral::Id();
}

} // namespace Settings
