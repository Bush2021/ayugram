// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

#include "ayu/secret/secret_chat_state.h"

#include "secret_scheme.h"

namespace AyuSecret {

struct IncomingLayer {
	SecretTLDecryptedMessage message;
	int32 layer = 0;
	int32 inSeqNo = 0;
	int32 outSeqNo = 0;
	std::optional<EncryptedFileInfo> file;
};

[[nodiscard]] std::optional<IncomingLayer> ParseLayer(
	bytes::const_span serialized);

enum class SeqCheck {
	Accept,
	Duplicate,
	Hole,
	Invalid,
};

[[nodiscard]] SeqCheck CheckSeqNo(
	const ChatState &state,
	int32 inSeqNo,
	int32 outSeqNo);

[[nodiscard]] int32 SeqIndex(int32 seqNo);
[[nodiscard]] int32 PeerOutSeqNo(const ChatState &state, int32 index);

[[nodiscard]] bool IsService(const SecretTLDecryptedMessage &message);

} // namespace AyuSecret
