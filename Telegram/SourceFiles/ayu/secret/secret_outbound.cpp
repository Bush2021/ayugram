// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/secret_outbound.h"

#include "ayu/secret/data_secret_chat.h"
#include "ayu/secret/secret_crypto.h"

namespace AyuSecret {
namespace {

constexpr auto kLayerRandomSize = 31;

} // namespace

int32 OutSeqNo(const ChatState &state) {
	const auto x = state.creator ? 0 : 1;
	return 2 * state.outCount + 1 - x;
}

int32 InSeqNo(const ChatState &state) {
	const auto x = state.creator ? 0 : 1;
	return 2 * state.inCount + x;
}

int32 WrapperLayer(const ChatState &state) {
	// Every payload is built with the layer 46 message constructor and the
	// layer 73 encryption, so the wrapper never announces less than the
	// default, whatever the peer told us about itself. An older client is
	// out of the fork's scope, and announcing 73 to one is what makes that
	// visible instead of silently sending it a body it cannot read.
	return std::max(
		int(SecretChatData::kDefaultLayer),
		std::min(int(kCurrentLayer), state.layerHis));
}

QByteArray SerializeMessage(const SecretTLDecryptedMessage &message) {
	auto buffer = mtpBuffer();
	message.write(buffer);
	auto result = QByteArray(
		reinterpret_cast<const char*>(buffer.constData()),
		int(buffer.size() * sizeof(mtpPrime)));
	bytes::set_random(bytes::make_span(buffer.data(), buffer.size()));
	return result;
}

std::optional<SecretTLDecryptedMessage> ParseMessage(
		const QByteArray &serialized) {
	const auto size = int(serialized.size());
	if (!size || (size % sizeof(mtpPrime)) != 0) {
		return std::nullopt;
	}
	auto buffer = mtpBuffer(size / sizeof(mtpPrime));
	const auto clear = gsl::finally([&] {
		bytes::set_random(bytes::make_span(buffer.data(), buffer.size()));
	});
	bytes::copy(
		bytes::make_span(buffer.data(), buffer.size()),
		bytes::make_span(serialized));
	auto from = buffer.constData();
	const auto end = from + buffer.size();
	auto result = SecretTLDecryptedMessage();
	if (!result.read(from, end) || from != end) {
		return std::nullopt;
	}
	return result;
}

QByteArray SerializeLayer(
		const ChatState &state,
		const SecretTLDecryptedMessage &message,
		int32 inSeqNo,
		int32 outSeqNo) {
	auto random = bytes::vector(kLayerRandomSize);
	bytes::set_random(bytes::make_span(random));

	auto buffer = mtpBuffer();
	SecretTLDecryptedMessageLayer(secret_decryptedMessageLayer(
		secret_bytes(random),
		secret_int(WrapperLayer(state)),
		secret_int(inSeqNo),
		secret_int(outSeqNo),
		message)).write(buffer);
	const auto serialized = bytes::make_span(
		buffer.constData(),
		buffer.size());
	auto result = QByteArray(
		reinterpret_cast<const char*>(serialized.data()),
		int(serialized.size()));
	bytes::set_random(bytes::make_span(buffer.data(), buffer.size()));
	return result;
}

OutgoingPayload PrepareOutgoing(
		const ChatState &state,
		const SecretTLDecryptedMessage &message) {
	auto result = OutgoingPayload();
	if (int(state.key.size()) != Crypto::kKeySize) {
		return result;
	}
	result.inSeqNo = InSeqNo(state);
	result.outSeqNo = OutSeqNo(state);
	result.serialized = SerializeLayer(
		state,
		message,
		result.inSeqNo,
		result.outSeqNo);
	const auto serialized = bytes::make_span(result.serialized);
	result.encrypted = Crypto::EncryptPayload(
		bytes::make_span(state.key),
		state.keyFingerprint,
		state.creator,
		serialized);
	if (result.encrypted.empty()) {
		return OutgoingPayload();
	}
	return result;
}

} // namespace AyuSecret
