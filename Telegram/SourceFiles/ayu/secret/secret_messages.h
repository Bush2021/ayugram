// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

#include "ayu/secret/secret_chat_state.h"

#include "secret_scheme.h"

class HistoryItem;
class SecretChatData;

namespace Main {
class Session;
} // namespace Main

// The end-to-end scheme carries its own copy of `MessageEntity`, so the text
// of a secret message is converted here instead of through `Api::` helpers.
// The two conversions mirror `EntitiesToMTP` / `EntitiesFromMTP`, minus the
// entity kinds the end-to-end scheme never gained.

namespace AyuSecret {

[[nodiscard]] SecretTLVector<SecretTLMessageEntity> EntitiesToSecret(
	not_null<Main::Session*> session,
	const EntitiesInText &entities);
[[nodiscard]] EntitiesInText EntitiesFromSecret(
	not_null<Main::Session*> session,
	const QVector<SecretTLMessageEntity> &entities);

[[nodiscard]] SecretTLDecryptedMessage MakeTextMessage(
	not_null<Main::Session*> session,
	uint64 randomId,
	int32 ttl,
	const TextWithEntities &text,
	uint64 replyToRandomId,
	bool silent,
	std::optional<SecretTLDecryptedMessageMedia> media = std::nullopt,
	uint64 groupedId = 0);

[[nodiscard]] TextWithEntities TextFromSecret(
	not_null<Main::Session*> session,
	const SecretTLDdecryptedMessage &data);
[[nodiscard]] TextWithEntities TextFromSecret(
	not_null<Main::Session*> session,
	const SecretTLDdecryptedMessage46 &data);

void RemoveHistoryItems(
	not_null<SecretChatData*> peer,
	const std::vector<MsgId> &ids);

HistoryItem *AddHistoryItem(
	not_null<SecretChatData*> peer,
	const HistoryRecord &record);

// WHY: records are ordered by id, and the whole history goes in as one
// slice, because adding stored items one by one as the last message marks
// the bottom unloaded and leaves all but the first outside the view.
void AddStoredHistory(
	not_null<SecretChatData*> peer,
	const std::vector<HistoryRecord> &records);

} // namespace AyuSecret
