// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

class SecretChatData;

namespace Ui {
class GenericBox;
} // namespace Ui

namespace AyuSecret {

void SecretKeyBox(
	not_null<Ui::GenericBox*> box,
	not_null<SecretChatData*> peer);

} // namespace AyuSecret
