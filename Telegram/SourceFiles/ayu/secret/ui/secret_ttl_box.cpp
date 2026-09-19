// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/ui/secret_ttl_box.h"

#include "ayu/secret/data_secret_chat.h"
#include "ayu/secret/secret_chats.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/boxes/time_picker_box.h"
#include "ui/layers/generic_box.h"
#include "ui/widgets/labels.h"
#include "styles/style_layers.h"

namespace AyuSecret {
namespace {

constexpr auto kMinute = TimeId(60);
constexpr auto kHour = TimeId(3600);
constexpr auto kDay = TimeId(86400);
constexpr auto kWeek = TimeId(86400 * 7);

[[nodiscard]] std::vector<TimeId> TtlValues() {
	auto result = std::vector<TimeId>();
	for (auto seconds = TimeId(1); seconds <= 15; ++seconds) {
		result.push_back(seconds);
	}
	result.insert(end(result), { 30, kMinute, kHour, kDay, kWeek });
	return result;
}

} // namespace

QString FormatTtl(TimeId ttl) {
	return (ttl < kMinute)
		? tr::lng_seconds(tr::now, lt_count, ttl)
		: (ttl < kHour)
		? tr::lng_minutes(tr::now, lt_count, ttl / kMinute)
		: (ttl < kDay)
		? tr::lng_hours(tr::now, lt_count, ttl / kHour)
		: (ttl < kWeek)
		? tr::lng_days(tr::now, lt_count, ttl / kDay)
		: tr::lng_weeks(tr::now, lt_count, ttl / kWeek);
}

QString FormatTtlTiny(TimeId ttl) {
	return (ttl < kMinute)
		? tr::lng_seconds_tiny(tr::now, lt_count, ttl)
		: (ttl < kHour)
		? tr::lng_minutes_tiny(tr::now, lt_count, ttl / kMinute)
		: (ttl < kDay)
		? tr::lng_hours_tiny(tr::now, lt_count, ttl / kHour)
		: (ttl < kWeek)
		? tr::lng_days_tiny(tr::now, lt_count, ttl / kDay)
		: tr::lng_weeks_tiny(tr::now, lt_count, ttl / kWeek);
}

void SecretTtlBox(
		not_null<Ui::GenericBox*> box,
		not_null<SecretChatData*> peer) {
	const auto user = peer->user();
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::lng_ttl_edit_about(
			lt_user,
			rpl::single(user ? user->shortName() : peer->shortName())),
		st::boxLabel));

	const auto ttls = TtlValues();
	const auto phrases = ranges::views::all(
		ttls
	) | ranges::views::transform(FormatTtl) | ranges::to_vector;
	const auto current = TimeId(peer->messagesTTL());
	const auto picked = Ui::TimePickerBox(box, ttls, phrases, current);

	const auto apply = [=](TimeId ttl) {
		peer->session().ayuSecret().setTtl(peer, ttl);
	};
	Ui::ConfirmBox(box, {
		.confirmed = [=](Fn<void()> close) {
			apply(picked());
			close();
		},
		.confirmText = tr::lng_settings_save(),
		.cancelText = tr::lng_cancel(),
	});

	box->setTitle(tr::lng_manage_messages_ttl_title());

	if (current) {
		box->addLeftButton(tr::lng_manage_messages_ttl_disable(), [=] {
			apply(0);
			box->closeBox();
		});
	}
}

} // namespace AyuSecret
