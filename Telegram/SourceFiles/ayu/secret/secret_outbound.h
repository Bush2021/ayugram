// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

#include "ayu/secret/secret_chat_state.h"

#include "secret_scheme.h"

// The outbound half of the protocol that holds no state of its own. `Chats`
// owns the counters and the queue, so everything here reads a `ChatState`
// and changes nothing in it: the caller decides when a message is really
// sent and only then moves the counters forward.

namespace AyuSecret {

inline constexpr auto kCurrentLayer = 144;

struct QueuedMessage {
	SecretTLDecryptedMessage message;
	uint64 randomId = 0;
	bool silent = false;
	bool service = false;
	std::optional<MTPInputEncryptedFile> file;
	std::optional<HistoryRecord> record;
	FullMsgId placeholder;
};

struct OutgoingPayload {
	QByteArray serialized;
	bytes::vector encrypted;
	int32 inSeqNo = 0;
	int32 outSeqNo = 0;
};

[[nodiscard]] int32 OutSeqNo(const ChatState &state);
[[nodiscard]] int32 InSeqNo(const ChatState &state);
[[nodiscard]] int32 WrapperLayer(const ChatState &state);

[[nodiscard]] QByteArray SerializeMessage(
	const SecretTLDecryptedMessage &message);
[[nodiscard]] std::optional<SecretTLDecryptedMessage> ParseMessage(
	const QByteArray &serialized);
[[nodiscard]] QByteArray SerializeLayer(
	const ChatState &state,
	const SecretTLDecryptedMessage &message,
	int32 inSeqNo,
	int32 outSeqNo);

// Wraps `message` in `decryptedMessageLayer`, serializes it boxed and
// encrypts the result. Returns an empty payload when the chat has no usable
// key. `serialized` is kept so that a resend request can be answered.
[[nodiscard]] OutgoingPayload PrepareOutgoing(
	const ChatState &state,
	const SecretTLDecryptedMessage &message);

} // namespace AyuSecret
