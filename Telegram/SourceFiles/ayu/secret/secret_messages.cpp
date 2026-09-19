// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/secret_messages.h"

#include "api/api_text_entities.h"
#include "ayu/secret/data_secret_chat.h"
#include "ayu/secret/secret_media.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "data/stickers/data_custom_emoji.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/basic_click_handlers.h"

namespace AyuSecret {
namespace {

[[nodiscard]] bool IsInternalUrl(const QString &url) {
	return url.startsWith(u"internal:"_q, Qt::CaseInsensitive);
}

[[nodiscard]] std::optional<SecretTLMessageEntity> MentionNameEntity(
		not_null<Main::Session*> session,
		SecretTLint offset,
		SecretTLint length,
		const QString &data) {
	const auto parsed = TextUtilities::MentionNameDataToFields(data);
	if (!parsed.userId || parsed.selfId != session->userId().bare) {
		return {};
	} else if (parsed.userId > uint64(std::numeric_limits<int32>::max())) {
		// The end-to-end scheme never widened `user_id` past 32 bits, so a
		// mention of a user with a wider id has no form here. Dropping the
		// entity leaves the name in the message as plain text.
		return {};
	}
	return secret_messageEntityMentionName(
		offset,
		length,
		secret_int(int32(parsed.userId)));
}

[[nodiscard]] std::optional<SecretTLMessageEntity> CustomEmojiEntity(
		SecretTLint offset,
		SecretTLint length,
		const QString &data) {
	const auto parsed = Data::ParseCustomEmojiData(data);
	if (!parsed) {
		return {};
	}
	return secret_messageEntityCustomEmoji(offset, length, secret_long(parsed));
}

} // namespace

SecretTLVector<SecretTLMessageEntity> EntitiesToSecret(
		not_null<Main::Session*> session,
		const EntitiesInText &entities) {
	auto v = QVector<SecretTLMessageEntity>();
	v.reserve(entities.size());
	for (const auto &entity : entities) {
		if (entity.length() <= 0) {
			continue;
		}
		const auto offset = secret_int(entity.offset());
		const auto length = secret_int(entity.length());
		switch (entity.type()) {
		case EntityType::Mention: {
			v.push_back(secret_messageEntityMention(offset, length));
		} break;
		case EntityType::Hashtag: {
			v.push_back(secret_messageEntityHashtag(offset, length));
		} break;
		case EntityType::BotCommand: {
			v.push_back(secret_messageEntityBotCommand(offset, length));
		} break;
		case EntityType::Url: {
			v.push_back(secret_messageEntityUrl(offset, length));
		} break;
		case EntityType::Email: {
			v.push_back(secret_messageEntityEmail(offset, length));
		} break;
		case EntityType::Phone: {
			v.push_back(secret_messageEntityPhone(offset, length));
		} break;
		case EntityType::Cashtag: {
			v.push_back(secret_messageEntityCashtag(offset, length));
		} break;
		case EntityType::BankCard: {
			v.push_back(secret_messageEntityBankCard(offset, length));
		} break;
		case EntityType::Bold: {
			v.push_back(secret_messageEntityBold(offset, length));
		} break;
		case EntityType::Italic: {
			v.push_back(secret_messageEntityItalic(offset, length));
		} break;
		case EntityType::Underline: {
			v.push_back(secret_messageEntityUnderline(offset, length));
		} break;
		case EntityType::StrikeOut: {
			v.push_back(secret_messageEntityStrike(offset, length));
		} break;
		case EntityType::Spoiler: {
			v.push_back(secret_messageEntitySpoiler(offset, length));
		} break;
		case EntityType::Blockquote: {
			v.push_back(secret_messageEntityBlockquote(offset, length));
		} break;
		case EntityType::Code: {
			v.push_back(secret_messageEntityCode(offset, length));
		} break;
		case EntityType::Pre: {
			v.push_back(secret_messageEntityPre(
				offset,
				length,
				secret_string(entity.data())));
		} break;
		case EntityType::CustomUrl: {
			const auto external = UrlClickHandler::ExternalUrlFromInternalUrl(
				entity.data());
			const auto url = external.isEmpty() ? entity.data() : external;
			if (!IsInternalUrl(url)) {
				v.push_back(secret_messageEntityTextUrl(
					offset,
					length,
					secret_string(url)));
			}
		} break;
		case EntityType::MentionName: {
			const auto valid = MentionNameEntity(
				session,
				offset,
				length,
				entity.data());
			if (valid) {
				v.push_back(*valid);
			}
		} break;
		case EntityType::CustomEmoji: {
			const auto valid = CustomEmojiEntity(
				offset,
				length,
				entity.data());
			if (valid) {
				v.push_back(*valid);
			}
		} break;
		}
	}
	return secret_vector<SecretTLMessageEntity>(std::move(v));
}

EntitiesInText EntitiesFromSecret(
		not_null<Main::Session*> session,
		const QVector<SecretTLMessageEntity> &entities) {
	if (entities.isEmpty()) {
		return {};
	}
	auto result = EntitiesInText();
	result.reserve(entities.size());
	for (const auto &entity : entities) {
		entity.match([&](const SecretTLDmessageEntityMention &d) {
			result.push_back({
				EntityType::Mention,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntityHashtag &d) {
			result.push_back({
				EntityType::Hashtag,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntityBotCommand &d) {
			result.push_back({
				EntityType::BotCommand,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntityUrl &d) {
			result.push_back({
				EntityType::Url,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntityEmail &d) {
			result.push_back({
				EntityType::Email,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntityPhone &d) {
			result.push_back({
				EntityType::Phone,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntityCashtag &d) {
			result.push_back({
				EntityType::Cashtag,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntityBankCard &d) {
			result.push_back({
				EntityType::BankCard,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntityBold &d) {
			result.push_back({
				EntityType::Bold,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntityItalic &d) {
			result.push_back({
				EntityType::Italic,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntityUnderline &d) {
			result.push_back({
				EntityType::Underline,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntityStrike &d) {
			result.push_back({
				EntityType::StrikeOut,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntitySpoiler &d) {
			result.push_back({
				EntityType::Spoiler,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntityBlockquote &d) {
			result.push_back({
				EntityType::Blockquote,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntityCode &d) {
			result.push_back({
				EntityType::Code,
				d.voffset().v,
				d.vlength().v,
			});
		}, [&](const SecretTLDmessageEntityPre &d) {
			result.push_back({
				EntityType::Pre,
				d.voffset().v,
				d.vlength().v,
				qs(d.vlanguage()),
			});
		}, [&](const SecretTLDmessageEntityTextUrl &d) {
			const auto url = qs(d.vurl());
			if (IsInternalUrl(url)) {
				return;
			}
			result.push_back({
				EntityType::CustomUrl,
				d.voffset().v,
				d.vlength().v,
				url,
			});
		}, [&](const SecretTLDmessageEntityMentionName &d) {
			const auto userId = UserId(BareId(uint32(d.vuser_id().v)));
			const auto user = session->data().userLoaded(userId);
			result.push_back({
				EntityType::MentionName,
				d.voffset().v,
				d.vlength().v,
				TextUtilities::MentionNameDataFromFields({
					.selfId = session->userId().bare,
					.userId = userId.bare,
					.accessHash = user ? user->accessHash() : 0,
				}),
			});
		}, [&](const SecretTLDmessageEntityCustomEmoji &d) {
			result.push_back({
				EntityType::CustomEmoji,
				d.voffset().v,
				d.vlength().v,
				Data::SerializeCustomEmojiId(d.vdocument_id().v),
			});
		}, [](const auto &) {
			// messageEntityUnknown, and every kind a newer layer may add.
		});
	}
	return result;
}

SecretTLDecryptedMessage MakeTextMessage(
		not_null<Main::Session*> session,
		uint64 randomId,
		int32 ttl,
		const TextWithEntities &text,
		uint64 replyToRandomId,
		bool silent,
		std::optional<SecretTLDecryptedMessageMedia> media,
		uint64 groupedId) {
	using Flag = SecretTLDdecryptedMessage::Flag;
	auto entities = EntitiesToSecret(session, text.entities);
	const auto flags = Flag()
		| (silent ? Flag::f_silent : Flag())
		| (media ? Flag::f_media : Flag())
		| (entities.v.isEmpty() ? Flag() : Flag::f_entities)
		| (replyToRandomId ? Flag::f_reply_to_random_id : Flag())
		| (groupedId ? Flag::f_grouped_id : Flag());
	return secret_decryptedMessage(
		secret_flags(flags),
		secret_long(randomId),
		secret_int(ttl),
		secret_string(text.text),
		media.value_or(SecretTLDecryptedMessageMedia()),
		entities,
		SecretTLstring(), // via_bot_name
		secret_long(replyToRandomId),
		secret_long(groupedId));
}

namespace {

template <typename Data>
[[nodiscard]] TextWithEntities TextFromData(
		not_null<Main::Session*> session,
		const Data &data) {
	auto result = TextWithEntities{ qs(data.vmessage()) };
	if (const auto entities = data.ventities()) {
		result.entities = EntitiesFromSecret(session, entities->v);
	}
	return result;
}

[[nodiscard]] MTPMessage MessageFromRecord(
		not_null<SecretChatData*> peer,
		not_null<UserData*> user,
		const HistoryRecord &record) {
	const auto session = &peer->session();
	const auto from = record.out ? session->user() : user;

	using Flag = MTPDmessage::Flag;
	auto entities = Api::EntitiesToMTP(
		session,
		record.text.entities,
		Api::ConvertOption::SkipLocal);
	auto reply = MTPMessageReplyHeader();
	if (record.replyToLocalId) {
		reply = MTP_messageReplyHeader(
			MTP_flags(MTPDmessageReplyHeader::Flag::f_reply_to_msg_id),
			MTP_int(record.replyToLocalId),
			MTPPeer(), // reply_to_peer_id
			MTPMessageFwdHeader(), // reply_from
			MTPMessageMedia(), // reply_media
			MTPint(), // reply_to_top_id
			MTPstring(), // quote_text
			MTPVector<MTPMessageEntity>(), // quote_entities
			MTPint(), // quote_offset
			MTPint(), // todo_item_id
			MTPbytes()); // poll_option
	}
	auto media = MediaFromRecord(session, record);
	const auto flags = Flag::f_from_id
		| (record.out ? Flag::f_out : Flag())
		| (record.mediaUnread ? Flag::f_media_unread : Flag())
		| ((media.type() != mtpc_messageMediaEmpty) ? Flag::f_media : Flag())
		| (entities.v.isEmpty() ? Flag() : Flag::f_entities)
		| (record.replyToLocalId ? Flag::f_reply_to : Flag())
		| (record.groupedId ? Flag::f_grouped_id : Flag());
	return MTP_message(
		MTP_flags(flags),
		MTP_int(record.id),
		peerToMTP(from->id),
		MTPint(), // from_boosts_applied
		MTPstring(), // from_rank
		peerToMTP(peer->id),
		MTPPeer(), // saved_peer_id
		MTPMessageFwdHeader(),
		MTPlong(), // via_bot_id
		MTPlong(), // via_business_bot_id
		MTPPeer(), // guestchat_via_from
		reply,
		MTP_int(record.date),
		MTP_string(record.text.text),
		media,
		MTPReplyMarkup(),
		entities,
		MTPint(), // views
		MTPint(), // forwards
		MTPMessageReplies(),
		MTPint(), // edit_date
		MTPstring(), // post_author
		MTP_long(record.groupedId),
		MTPMessageReactions(),
		MTPVector<MTPRestrictionReason>(),
		MTPint(), // ttl_period
		MTPint(), // quick_reply_shortcut_id
		MTPlong(), // effect
		MTPFactCheck(),
		MTPint(), // report_delivery_until_date
		MTPlong(), // paid_message_stars
		MTPSuggestedPost(),
		MTPint(), // schedule_repeat_period
		MTPstring(), // summary_from_language
		MTPRichMessage());
}

[[nodiscard]] MTPMessageAction ServiceAction(const HistoryRecord &record) {
	switch (record.serviceKind) {
	case ServiceKind::SetTtl:
		return MTP_messageActionSetMessagesTTL(
			MTP_flags(0),
			MTP_int(record.servicePayload.toInt()),
			MTPlong());
	case ServiceKind::Screenshot:
		return MTP_messageActionScreenshotTaken();
	case ServiceKind::FileRejected:
		return MTP_messageActionCustomAction(
			MTP_string(tr::ayu_SecretChatFileRejected(tr::now)));
	case ServiceKind::None:
		break;
	}
	return MTP_messageActionEmpty();
}

[[nodiscard]] MTPMessage ServiceFromRecord(
		not_null<SecretChatData*> peer,
		not_null<UserData*> user,
		const HistoryRecord &record) {
	const auto from = record.out ? peer->session().user() : user;

	using Flag = MTPDmessageService::Flag;
	return MTP_messageService(
		MTP_flags(Flag::f_from_id | (record.out ? Flag::f_out : Flag())),
		MTP_int(record.id),
		peerToMTP(from->id),
		peerToMTP(peer->id),
		MTPPeer(), // saved_peer_id
		MTPMessageReplyHeader(),
		MTP_int(record.date),
		ServiceAction(record),
		MTPMessageReactions(),
		MTPint()); // ttl_period
}

[[nodiscard]] MTPMessage FromRecord(
		not_null<SecretChatData*> peer,
		not_null<UserData*> user,
		const HistoryRecord &record) {
	return (record.serviceKind != ServiceKind::None)
		? ServiceFromRecord(peer, user, record)
		: MessageFromRecord(peer, user, record);
}

} // namespace

TextWithEntities TextFromSecret(
		not_null<Main::Session*> session,
		const SecretTLDdecryptedMessage &data) {
	return TextFromData(session, data);
}

TextWithEntities TextFromSecret(
		not_null<Main::Session*> session,
		const SecretTLDdecryptedMessage46 &data) {
	return TextFromData(session, data);
}

void RemoveHistoryItems(
		not_null<SecretChatData*> peer,
		const std::vector<MsgId> &ids) {
	auto &owner = peer->owner();
	auto items = std::vector<not_null<HistoryItem*>>();
	for (const auto id : ids) {
		if (const auto item = owner.message(peer->id, id)) {
			items.push_back(item);
		}
	}
	if (items.empty()) {
		return;
	}
	owner.notifyItemsAboutToBeDestroyed(items);
	for (const auto &item : items) {
		item->destroy();
	}
}

HistoryItem *AddHistoryItem(
		not_null<SecretChatData*> peer,
		const HistoryRecord &record) {
	const auto user = peer->user();
	if (!record.id || !user) {
		return nullptr;
	}
	return peer->owner().history(peer)->addNewMessage(
		record.id,
		FromRecord(peer, user, record),
		MessageFlags(),
		NewMessageType::Unread);
}

void AddStoredHistory(
		not_null<SecretChatData*> peer,
		const std::vector<HistoryRecord> &records) {
	const auto user = peer->user();
	if (!user) {
		return;
	}
	auto slice = QVector<MTPMessage>();
	slice.reserve(records.size());
	for (const auto &record : records | ranges::views::reverse) {
		if (record.id) {
			slice.push_back(FromRecord(peer, user, record));
		}
	}
	peer->owner().history(peer)->addOlderSlice(slice);
}

} // namespace AyuSecret
