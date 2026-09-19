// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

class SecretChatData;

namespace AyuSecret {

struct Status {
	QString text;
	bool active = false;
};

[[nodiscard]] Status StatusText(not_null<SecretChatData*> peer, TimeId now);

} // namespace AyuSecret
