// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/secret_inbound.h"

namespace AyuSecret {

std::optional<IncomingLayer> ParseLayer(bytes::const_span serialized) {
	const auto size = int(serialized.size());
	if (!size || (size % sizeof(mtpPrime)) != 0) {
		return std::nullopt;
	}
	auto buffer = mtpBuffer(size / sizeof(mtpPrime));
	const auto clear = gsl::finally([&] {
		bytes::set_random(bytes::make_span(buffer.data(), buffer.size()));
	});
	bytes::copy(bytes::make_span(buffer.data(), buffer.size()), serialized);

	auto from = buffer.constData();
	const auto end = from + buffer.size();
	auto layer = SecretTLDecryptedMessageLayer();
	if (!layer.read(from, end) || from != end) {
		return std::nullopt;
	}
	const auto &data = layer.data();
	return IncomingLayer{
		.message = data.vmessage(),
		.layer = data.vlayer().v,
		.inSeqNo = data.vin_seq_no().v,
		.outSeqNo = data.vout_seq_no().v,
	};
}

SeqCheck CheckSeqNo(const ChatState &state, int32 inSeqNo, int32 outSeqNo) {
	const auto x = state.creator ? 0 : 1;
	if (inSeqNo < 0
		|| outSeqNo < 0
		|| (inSeqNo % 2) != (1 - x)
		|| (outSeqNo % 2) != x
		|| SeqIndex(inSeqNo) > state.outCount) {
		return SeqCheck::Invalid;
	}
	const auto index = SeqIndex(outSeqNo);
	return (index < state.inCount)
		? SeqCheck::Duplicate
		: (index > state.inCount)
		? SeqCheck::Hole
		: (inSeqNo < state.hisInSeqNo)
		? SeqCheck::Invalid
		: SeqCheck::Accept;
}

int32 SeqIndex(int32 seqNo) {
	return seqNo / 2;
}

int32 PeerOutSeqNo(const ChatState &state, int32 index) {
	const auto x = state.creator ? 0 : 1;
	return 2 * index + x;
}

bool IsService(const SecretTLDecryptedMessage &message) {
	return (message.type() == secretc_decryptedMessageService)
		|| (message.type() == secretc_decryptedMessageService8);
}

} // namespace AyuSecret
