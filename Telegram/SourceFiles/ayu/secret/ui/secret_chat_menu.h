// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

#include "window/window_peer_menu.h"

namespace Window {
class SessionController;
} // namespace Window

namespace AyuSecret {

[[nodiscard]] bool CanStartSecretChat(not_null<UserData*> user);

void StartSecretChat(
	not_null<Window::SessionController*> controller,
	not_null<UserData*> user);

void ShowNewSecretChatBox(
	not_null<Window::SessionController*> controller);

void AddSecretChatActions(
	PeerData *peer,
	not_null<Window::SessionController*> controller,
	const Window::PeerMenuCallback &addCallback);

} // namespace AyuSecret
