// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

class UserData;

namespace Window {
class SessionController;
} // namespace Window

namespace AyuUi {

void ShowCommonMessagesBox(
	not_null<Window::SessionController*> controller,
	not_null<UserData*> user);

} // namespace AyuUi
