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
#include "main/main_session.h"
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
		requestBatch(
			{ std::move(request) },
			to,
			[done = std::move(done)](int, Ui::TranslateProviderResult result) {
				done(std::move(result));
			},
			[] {});
	}

	void requestBatch(
			std::vector<Ui::TranslateProviderRequest> requests,
			const LanguageId &to,
			Fn<void(int, Ui::TranslateProviderResult)> doneOne,
			Fn<void()> doneAll) override {
		using Flag = MTPmessages_TranslateText::Flag;
		const auto manager = Ayu::Translator::TranslateManager::currentInstance();
		if (requests.empty()) {
			doneAll();
			return;
		}

		const auto failAll = [=] {
			for (auto i = 0; i != requests.size(); ++i) {
				doneOne(i, Ui::TranslateProviderResult{
					.error = Ui::TranslateProviderError::Unknown,
				});
			}
			doneAll();
		};
		if (!manager) {
			failAll();
			return;
		}
		const auto doneFromList = [=, session = _session](
				const QVector<MTPTextWithEntities> &list) {
			for (auto i = 0; i != requests.size(); ++i) {
				doneOne(
					i,
					(i < list.size())
						? Ui::TranslateProviderResult{
							.text = Api::ParseTextWithEntities(session, list[i]),
						}
						: Ui::TranslateProviderResult{
							.error = Ui::TranslateProviderError::Unknown,
						});
			}
			doneAll();
		};

		const auto firstPeer = PeerId(requests.front().peerId);
		auto allWithIds = true;
		for (const auto &request : requests) {
			if ((PeerId(request.peerId) != firstPeer) || (request.msgId == 0)) {
				allWithIds = false;
				break;
			}
		}

		if (allWithIds) {
			const auto peer = _session->data().peerLoaded(firstPeer);
			if (!peer) {
				failAll();
				return;
			}
			auto ids = QVector<MTPint>();
			ids.reserve(requests.size());
			for (const auto &request : requests) {
				ids.push_back(MTP_int(MsgId(request.msgId)));
			}
			manager->request(
				_session,
				MTP_flags(Flag::f_peer | Flag::f_id),
				peer->input(),
				MTP_vector<MTPint>(ids),
				MTPVector<MTPTextWithEntities>(),
				MTP_string(to.twoLetterCode())
			).done([=](const MTPmessages_TranslatedText &result) {
				doneFromList(result.data().vresult().v);
			}).fail([=](const MTP::Error &) {
				failAll();
			}).send();
			return;
		}

		auto allWithText = true;
		for (const auto &request : requests) {
			if (request.text.text.isEmpty()) {
				allWithText = false;
				break;
			}
		}
		if (!allWithText) {
			Ui::TranslateProvider::requestBatch(
				std::move(requests),
				to,
				std::move(doneOne),
				std::move(doneAll));
			return;
		}

		auto text = QVector<MTPTextWithEntities>();
		text.reserve(requests.size());
		for (const auto &request : requests) {
			text.push_back(MTP_textWithEntities(
				MTP_string(request.text.text),
				Api::EntitiesToMTP(
					_session,
					request.text.entities,
					Api::ConvertOption::SkipLocal)));
		}
		manager->request(
			_session,
			MTP_flags(Flag::f_text),
			MTP_inputPeerEmpty(),
			MTPVector<MTPint>(),
			MTP_vector<MTPTextWithEntities>(text),
			MTP_string(to.twoLetterCode())
		).done([=](const MTPmessages_TranslatedText &result) {
			doneFromList(result.data().vresult().v);
		}).fail([=](const MTP::Error &) {
			failAll();
		}).send();
	}

private:
	const not_null<Main::Session*> _session;
};

} // namespace

namespace Ui {

std::unique_ptr<TranslateProvider> CreateTranslateProvider(
		not_null<Main::Session*> session) {
	const auto urlTemplate = OptionTranslateUrlTemplate.value();
	if (!urlTemplate.isEmpty()
		&& urlTemplate.contains(u"%q"_q)) {
		return CreateUrlTranslateProvider(urlTemplate);
	}
	if (AyuSettings::getInstance().translationProvider()
		!= TranslationProvider::Telegram) {
		return std::make_unique<AyuTranslateProvider>(session);
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
