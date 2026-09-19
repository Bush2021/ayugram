// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

#include "data/data_peer.h"

class SecretChatData final : public PeerData {
public:
	enum class State : uchar {
		Requested,
		Waiting,
		Ready,
		Closed,
	};

	static constexpr auto kDefaultLayer = 73;

	SecretChatData(not_null<Data::Session*> owner, PeerId id);

	[[nodiscard]] SecretChatId secretChatId() const {
		return peerToSecretChat(id);
	}
	[[nodiscard]] int32 serverChatId() const {
		return SecretChatIdToServer(secretChatId());
	}

	[[nodiscard]] State state() const {
		return _state.current();
	}
	[[nodiscard]] rpl::producer<State> stateValue() const {
		return _state.value();
	}
	[[nodiscard]] bool isReady() const {
		return (state() == State::Ready);
	}
	void setState(State state);

	[[nodiscard]] UserData *user() const {
		return _user;
	}
	void setUser(not_null<UserData*> user);

	[[nodiscard]] bool creator() const {
		return _creator;
	}
	void setCreator(bool creator) {
		_creator = creator;
	}

	[[nodiscard]] int ttl() const {
		return _ttl;
	}
	void setTtl(int ttl) {
		_ttl = ttl;
		setMessagesTTL(ttl);
	}

	[[nodiscard]] int layerHis() const {
		return _layerHis;
	}
	void setLayerHis(int layer) {
		_layerHis = layer;
	}

private:
	void mirrorUserName();
	void refreshChatFilters();
	void mirrorUserUserpic();

	UserData *_user = nullptr;
	rpl::variable<State> _state = State::Requested;
	int _ttl = 0;
	int _layerHis = kDefaultLayer;
	bool _creator = false;
	rpl::lifetime _userLifetime;

};
