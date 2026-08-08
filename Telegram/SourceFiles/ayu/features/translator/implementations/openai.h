// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Radolyn, 2026
#pragma once

#include "ayu/features/translator/implementations/base.h"

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtNetwork/QNetworkAccessManager>

namespace Ayu::Translator {

class OpenAITranslator final : public BaseTranslator {
	Q_OBJECT

public:
	static OpenAITranslator &Instance();

	[[nodiscard]] QSet<QString> supportedLanguages() const override { return {}; }
	void startTranslation(
		const StartTranslationArgs &args
	) override;

private:
	explicit OpenAITranslator(QObject *parent = nullptr);

	QNetworkAccessManager _nam;

};

} // namespace Ayu::Translator
