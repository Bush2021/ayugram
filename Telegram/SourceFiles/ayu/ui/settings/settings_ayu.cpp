// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2025
#include "settings_ayu.h"

#include "lang_auto.h"
#include "settings_ayu_utils.h"
#include "settings_main.h"
#include "ayu/ayu_settings.h"
#include "boxes/peer_list_box.h"
#include "filters/settings_filters_list.h"
#include "settings/settings_common.h"
#include "settings/settings_builder.h"
#include "styles/style_settings.h"
#include "styles/style_menu_icons.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"

namespace Settings {

using namespace Builder;

rpl::producer<QString> AyuGhost::title() {
	return rpl::single(QString("AyuGram"));
}

AyuGhost::AyuGhost(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
	: Section(parent, controller) {
	setupContent(controller);
}

void SetupGhostModeToggle(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container, tr::ayu_GhostEssentialsHeader());

	std::vector checkboxes{
		NestedEntry{
			tr::ayu_DontReadMessages(tr::now), !settings->sendReadMessages, [=](bool enabled)
			{
				AyuSettings::set_sendReadMessages(!enabled);
				AyuSettings::save();
			}
		},
		NestedEntry{
			tr::ayu_DontReadStories(tr::now), !settings->sendReadStories, [=](bool enabled)
			{
				AyuSettings::set_sendReadStories(!enabled);
				AyuSettings::save();
			}
		},
		NestedEntry{
			tr::ayu_DontSendOnlinePackets(tr::now), !settings->sendOnlinePackets, [=](bool enabled)
			{
				AyuSettings::set_sendOnlinePackets(!enabled);
				AyuSettings::save();
			}
		},
		NestedEntry{
			tr::ayu_DontSendUploadProgress(tr::now), !settings->sendUploadProgress, [=](bool enabled)
			{
				AyuSettings::set_sendUploadProgress(!enabled);
				AyuSettings::save();
			}
		},
		NestedEntry{
			tr::ayu_SendOfflinePacketAfterOnline(tr::now), settings->sendOfflinePacketAfterOnline, [=](bool enabled)
			{
				AyuSettings::set_sendOfflinePacketAfterOnline(enabled);
				AyuSettings::save();
			}
		},
	};

	AddCollapsibleToggle(container, tr::ayu_GhostModeToggle(), checkboxes, true);
}

void SetupGhostEssentials(
	not_null<Ui::VerticalLayout*> container,
	not_null<AyuSettings::AyuGramSettings*> settings,
	not_null<rpl::variable<bool>*> markReadAfterActionVal,
	not_null<rpl::variable<bool>*> useScheduledMessagesVal) {
	SetupGhostModeToggle(container);


	AddButtonWithIcon(
		container,
		tr::ayu_MarkReadAfterAction(),
		st::settingsButtonNoIcon
	)->toggleOn(
		markReadAfterActionVal->value()
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->markReadAfterAction);
		}) | on_next(
		[=](bool enabled)
		{
			AyuSettings::set_markReadAfterAction(enabled);
			if (enabled) {
				AyuSettings::set_useScheduledMessages(false);
				useScheduledMessagesVal->force_assign(false);
			}

			AyuSettings::save();
		},
		container->lifetime());
	AddSkip(container);
	AddDividerText(container, tr::ayu_MarkReadAfterActionDescription());
}

void SetupScheduleMessages(
	not_null<Ui::VerticalLayout*> container,
	not_null<AyuSettings::AyuGramSettings*> settings,
	not_null<rpl::variable<bool>*> markReadAfterActionVal,
	not_null<rpl::variable<bool>*> useScheduledMessagesVal) {
	AddSkip(container);
	AddButtonWithIcon(
		container,
		tr::ayu_UseScheduledMessages(),
		st::settingsButtonNoIcon
	)->toggleOn(
		useScheduledMessagesVal->value()
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->useScheduledMessages);
		}) | on_next(
		[=](bool enabled)
		{
			AyuSettings::set_useScheduledMessages(enabled);
			if (enabled) {
				AyuSettings::set_markReadAfterAction(false);
				markReadAfterActionVal->force_assign(false);
			}

			AyuSettings::save();
		},
		container->lifetime());
	AddSkip(container);
	AddDividerText(container, tr::ayu_UseScheduledMessagesDescription());
}

void SetupSendWithoutSound(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	AddSkip(container);
	AddButtonWithIcon(
		container,
		tr::ayu_SendWithoutSoundByDefault(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->sendWithoutSound)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->sendWithoutSound);
		}) | on_next(
		[=](bool enabled)
		{
			AyuSettings::set_sendWithoutSound(enabled);
			AyuSettings::save();
		},
		container->lifetime());
	AddSkip(container);
	AddDividerText(container, tr::ayu_SendWithoutSoundByDefaultDescription());
}

void SetupSpyEssentials(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container, tr::ayu_SpyEssentialsHeader());

	AddButtonWithIcon(
		container,
		tr::ayu_SaveDeletedMessages(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->saveDeletedMessages)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->saveDeletedMessages);
		}) | on_next(
		[=](bool enabled)
		{
			AyuSettings::set_saveDeletedMessages(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_SaveMessagesHistory(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->saveMessagesHistory)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->saveMessagesHistory);
		}) | on_next(
		[=](bool enabled)
		{
			AyuSettings::set_saveMessagesHistory(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddSkip(container);
	AddDivider(container);
	AddSkip(container);

	AddButtonWithIcon(
		container,
		tr::ayu_MessageSavingSaveForBots(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->saveForBots)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->saveForBots);
		}) | on_next(
		[=](bool enabled)
		{
			AyuSettings::set_saveForBots(enabled);
			AyuSettings::save();
		},
		container->lifetime());
}

void SetupOther(not_null<Ui::VerticalLayout*> container) {
	auto *settings = &AyuSettings::getInstance();

	AddSubsectionTitle(container, tr::ayu_MessageSavingOtherHeader());

	AddButtonWithIcon(
		container,
		tr::ayu_LocalPremium(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->localPremium)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->localPremium);
		}) | on_next(
		[=](bool enabled)
		{
			AyuSettings::set_localPremium(enabled);
			AyuSettings::save();
		},
		container->lifetime());

	AddButtonWithIcon(
		container,
		tr::ayu_DisableAds(),
		st::settingsButtonNoIcon
	)->toggleOn(
		rpl::single(settings->disableAds)
	)->toggledValue(
	) | rpl::filter(
		[=](bool enabled)
		{
			return (enabled != settings->disableAds);
		}) | on_next(
		[=](bool enabled)
		{
			AyuSettings::set_disableAds(enabled);
			AyuSettings::save();
		},
		container->lifetime());
}

void AyuGhost::setupContent(not_null<Window::SessionController*> controller) {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);

	const auto settings = &AyuSettings::getInstance();
	const auto markReadAfterActionVal = content->lifetime().make_state<rpl::variable<bool>>(
		settings->markReadAfterAction);
	const auto useScheduledMessagesVal = content->lifetime().make_state<rpl::variable<bool>>(
		settings->useScheduledMessages);

	AddSkip(content);

	SetupGhostEssentials(content, settings, markReadAfterActionVal, useScheduledMessagesVal);
	SetupScheduleMessages(content, settings, markReadAfterActionVal, useScheduledMessagesVal);
	SetupSendWithoutSound(content);

	AddSkip(content);
	SetupSpyEssentials(content);

	AddSkip(content);
	AddDivider(content);
	AddSkip(content);

	SetupOther(content);
	AddSkip(content);

	ResizeFitChild(this, content);
}

const auto kMeta = BuildHelper({
	.id = AyuGhost::Id(),
	.parentId = AyuMain::Id(),
	.title = &tr::ayu_AyuPreferences,
	.icon = &st::menuIconGroupReactions,
}, [](SectionBuilder &builder) {
	// Ghost Mode settings
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"ayu/ghost-mode"_q,
			.title = tr::ayu_GhostModeToggle(tr::now),
			.keywords = { u"ghost"_q, u"invisible"_q, u"stealth"_q, u"privacy"_q },
		};
	});
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"ayu/dont-read-messages"_q,
			.title = tr::ayu_DontReadMessages(tr::now),
			.keywords = { u"read"_q, u"messages"_q, u"ghost"_q, u"privacy"_q },
		};
	});
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"ayu/dont-read-stories"_q,
			.title = tr::ayu_DontReadStories(tr::now),
			.keywords = { u"read"_q, u"stories"_q, u"ghost"_q, u"privacy"_q },
		};
	});
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"ayu/dont-send-online"_q,
			.title = tr::ayu_DontSendOnlinePackets(tr::now),
			.keywords = { u"online"_q, u"status"_q, u"ghost"_q, u"privacy"_q },
		};
	});
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"ayu/dont-send-upload"_q,
			.title = tr::ayu_DontSendUploadProgress(tr::now),
			.keywords = { u"upload"_q, u"typing"_q, u"progress"_q, u"ghost"_q },
		};
	});
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"ayu/mark-read-after-action"_q,
			.title = tr::ayu_MarkReadAfterAction(tr::now),
			.keywords = { u"read"_q, u"action"_q, u"mark"_q },
		};
	});
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"ayu/scheduled-messages"_q,
			.title = tr::ayu_UseScheduledMessages(tr::now),
			.keywords = { u"scheduled"_q, u"messages"_q, u"send"_q },
		};
	});
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"ayu/send-without-sound"_q,
			.title = tr::ayu_SendWithoutSoundByDefault(tr::now),
			.keywords = { u"sound"_q, u"silent"_q, u"mute"_q, u"notification"_q },
		};
	});

	// Spy Essentials
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"ayu/save-deleted"_q,
			.title = tr::ayu_SaveDeletedMessages(tr::now),
			.keywords = { u"deleted"_q, u"save"_q, u"messages"_q, u"spy"_q },
		};
	});
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"ayu/save-history"_q,
			.title = tr::ayu_SaveMessagesHistory(tr::now),
			.keywords = { u"history"_q, u"save"_q, u"messages"_q },
		};
	});
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"ayu/local-premium"_q,
			.title = tr::ayu_LocalPremium(tr::now),
			.keywords = { u"premium"_q, u"local"_q, u"fake"_q },
		};
	});
	builder.add(nullptr, [] {
		return SearchEntry{
			.id = u"ayu/disable-ads"_q,
			.title = tr::ayu_DisableAds(tr::now),
			.keywords = { u"ads"_q, u"disable"_q, u"block"_q, u"advertising"_q },
		};
	});
});

} // namespace Settings
