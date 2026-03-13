/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "lang/translate_provider.h"

#include "api/api_text_entities.h"
#include "ayu/ayu_settings.h"
#include "ayu/features/translator/ayu_translator.h"
#include "base/options.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "data/data_msg_id.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "history/history_item.h"
#include "lang/translate_mtproto_provider.h"
#include "lang/translate_url_provider.h"
#include "platform/platform_translate_provider.h"

namespace {

base::options::option<QString> OptionTranslateUrlTemplate({
	.id = "translate-url-template",
	.name = "Translate URL template",
	.description = "Template URL for custom translation provider."
		" Supports %q text, %f source language and %t target language.",
});

class AyuTranslateProvider final : public Ui::TranslateProvider {
public:
	explicit AyuTranslateProvider(not_null<Main::Session*> session)
	: _session(session) {
	}

	[[nodiscard]] bool supportsMessageId() const override {
		return true;
	}

	void request(
			Ui::TranslateProviderRequest request,
			LanguageId to,
			Fn<void(Ui::TranslateProviderResult)> done) override {
		using Flag = MTPmessages_TranslateText::Flag;
		const auto flags = request.msgId
			? (Flag::f_peer | Flag::f_id)
			: !request.text.text.isEmpty()
			? Flag::f_text
			: Flag(0);
		const auto peer = request.msgId
			? _session->data().peer(PeerId(request.peerId))->input()
			: MTP_inputPeerEmpty();
		const auto idList = request.msgId
			? MTP_vector<MTPint>(1, MTP_int(request.msgId))
			: MTPVector<MTPint>();
		const auto text = request.msgId
			? MTPVector<MTPTextWithEntities>()
			: MTP_vector<MTPTextWithEntities>(1, MTP_textWithEntities(
				MTP_string(request.text.text),
				Api::EntitiesToMTP(
					_session,
					request.text.entities,
					Api::ConvertOption::SkipLocal)));

		Ayu::Translator::TranslateManager::currentInstance()->request(
			_session,
			MTP_flags(flags),
			peer,
			idList,
			text,
			MTP_string(to.twoLetterCode())
		).done([=](const MTPmessages_TranslatedText &result) {
			const auto &data = result.data();
			const auto &list = data.vresult().v;
			if (list.isEmpty()) {
				done(Ui::TranslateProviderResult{
					.error = Ui::TranslateProviderError::Unknown,
				});
				return;
			}
			done(Ui::TranslateProviderResult{
				.text = Api::ParseTextWithEntities(_session, list.front()),
			});
		}).fail([=](const MTP::Error &) {
			done(Ui::TranslateProviderResult{
				.error = Ui::TranslateProviderError::Unknown,
			});
		}).send();
	}

private:
	not_null<Main::Session*> _session;
};

} // namespace

namespace Ui {

std::unique_ptr<TranslateProvider> CreateTranslateProvider(
		not_null<Main::Session*> session) {
	const auto ayuProvider = AyuSettings::getInstance().translationProvider;
	if (ayuProvider == u"google"_q || ayuProvider == u"yandex"_q) {
		return std::make_unique<AyuTranslateProvider>(session);
	}

	const auto urlTemplate = OptionTranslateUrlTemplate.value();
	if (!urlTemplate.isEmpty()
		&& urlTemplate.contains(u"%q"_q)) {
		return CreateUrlTranslateProvider(urlTemplate);
	}
	if (Core::App().settings().usePlatformTranslation()
		&& Platform::IsTranslateProviderAvailable()) {
		return Platform::CreateTranslateProvider();
	}
	return CreateMTProtoTranslateProvider(session);
}

TranslateProviderRequest PrepareTranslateProviderRequest(
		not_null<TranslateProvider*> provider,
		not_null<PeerData*> peer,
		MsgId msgId,
		TextWithEntities text) {
	auto result = TranslateProviderRequest{
		.peerId = uint64(peer->id.value),
		.msgId = IsServerMsgId(msgId) ? msgId.bare : 0,
		.text = std::move(text),
	};
	if (provider->supportsMessageId()) {
		return result;
	}
	if (result.msgId) {
		if (const auto i = peer->owner().message(peer, MsgId(result.msgId))) {
			result.text = i->originalText();
		}
		result.msgId = 0;
	}
	return result;
}

} // namespace Ui
