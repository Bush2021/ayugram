// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/data_secret_chat.h"

#include "data/data_changes.h"
#include "data/data_chat_filters.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "main/main_session.h"

namespace {

using UpdateFlag = Data::PeerUpdate::Flag;

} // namespace

SecretChatData::SecretChatData(not_null<Data::Session*> owner, PeerId id)
: PeerData(owner, id) {
}

void SecretChatData::setState(State state) {
	if (_state.current() == state) {
		return;
	}
	_state = state;
	session().changes().peerUpdated(this, UpdateFlag::OnlineStatus);
}

void SecretChatData::setUser(not_null<UserData*> user) {
	if (_user == user) {
		return;
	}
	_user = user;
	_userLifetime.destroy();
	mirrorUserName();
	mirrorUserUserpic();
	session().changes().peerUpdates(
		user,
		(UpdateFlag::Name
			| UpdateFlag::Photo
			| UpdateFlag::OnlineStatus
			| UpdateFlag::IsContact)
	) | rpl::on_next([=](const Data::PeerUpdate &update) {
		if (update.flags & UpdateFlag::Name) {
			mirrorUserName();
		}
		if (update.flags & UpdateFlag::Photo) {
			mirrorUserUserpic();
		}
		if (update.flags & UpdateFlag::OnlineStatus) {
			session().changes().peerUpdated(this, UpdateFlag::OnlineStatus);
		}
		if (update.flags & UpdateFlag::IsContact) {
			refreshChatFilters();
		}
	}, _userLifetime);
	refreshChatFilters();
}

void SecretChatData::refreshChatFilters() {
	if (const auto history = owner().historyLoaded(this)) {
		owner().chatsFilters().refreshHistory(history);
	}
}

void SecretChatData::mirrorUserName() {
	if (name() != _user->name()) {
		updateNameDelayed(_user->name(), QString(), QString());
	}
}

void SecretChatData::mirrorUserUserpic() {
	const auto photoId = _user->userpicPhotoId();
	const auto location = _user->userpicLocation();
	const auto hasVideo = _user->userpicHasVideo();
	const auto changed = (userpicPhotoId() != photoId)
		|| (userpicLocation() != location)
		|| (userpicHasVideo() != hasVideo);
	setUserpic(photoId, location, hasVideo);
	if (changed) {
		session().changes().peerUpdated(this, UpdateFlag::Photo);
	}
}
