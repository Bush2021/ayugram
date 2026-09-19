// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/secret_status.h"

#include "ayu/secret/data_secret_chat.h"
#include "data/data_peer_values.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"

namespace AyuSecret {

Status StatusText(not_null<SecretChatData*> peer, TimeId now) {
	const auto user = peer->user();
	switch (peer->state()) {
	case SecretChatData::State::Requested:
		return { tr::ayu_SecretChatExchangingKeys(tr::now) };
	case SecretChatData::State::Waiting:
		return { tr::ayu_SecretChatWaiting(
			tr::now,
			lt_user,
			user ? user->shortName() : QString()) };
	case SecretChatData::State::Closed:
		return { tr::ayu_SecretChatClosed(tr::now) };
	case SecretChatData::State::Ready:
		break;
	}
	if (!user) {
		return {};
	}
	return {
		.text = Data::OnlineText(user, now),
		.active = Data::OnlineTextActive(user, now),
	};
}

} // namespace AyuSecret
