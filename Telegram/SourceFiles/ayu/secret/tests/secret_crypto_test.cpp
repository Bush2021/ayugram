// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/secret_crypto.h"
#include "ayu/secret/secret_chat_state.h"

#include "core/utils.h"
#include "mtproto/mtproto_dh_utils.h"

#include <openssl/evp.h>

#include <cstring>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

auto FailedChecks = 0;
auto TotalChecks = 0;

void Check(bool condition, const char *name) {
	++TotalChecks;
	if (!condition) {
		++FailedChecks;
		std::cout << "FAILED: " << name << std::endl;
	}
}

[[nodiscard]] int HexNibble(char value) {
	if (value >= '0' && value <= '9') {
		return value - '0';
	} else if (value >= 'a' && value <= 'f') {
		return value - 'a' + 10;
	} else if (value >= 'A' && value <= 'F') {
		return value - 'A' + 10;
	}
	return -1;
}

[[nodiscard]] bytes::vector FromHex(std::string_view value) {
	if ((value.size() % 2) != 0) {
		return {};
	}
	auto result = bytes::vector(value.size() / 2);
	for (auto i = size_t(0); i != result.size(); ++i) {
		const auto high = HexNibble(value[2 * i]);
		const auto low = HexNibble(value[2 * i + 1]);
		if (high < 0 || low < 0) {
			return {};
		}
		result[i] = bytes::type((high << 4) | low);
	}
	return result;
}

[[nodiscard]] bytes::vector Sequence(int size) {
	auto result = bytes::vector(size);
	for (auto i = 0; i != size; ++i) {
		result[i] = bytes::type(i & 0xFF);
	}
	return result;
}

} // namespace

int32 *hashMd5(const void *data, uint32 len, void *dest) {
	auto written = 0U;
	const auto success = EVP_Digest(
		data,
		len,
		static_cast<unsigned char*>(dest),
		&written,
		EVP_md5(),
		nullptr);
	if (!success || written != 16) {
		std::memset(dest, 0, 16);
	}
	return static_cast<int32*>(dest);
}

namespace MTP {

bool IsPrimeAndGood(bytes::const_span, int) {
	return false;
}

bool IsGoodModExpFirst(
		const openssl::BigNum &,
		const openssl::BigNum &) {
	return false;
}

} // namespace MTP

namespace crl {

rpl::producer<> on_main_update_requests() {
	return rpl::never<>();
}

} // namespace crl

int main() {
	using namespace AyuSecret::Crypto;

	const auto key = Sequence(kKeySize);
	const auto fingerprint = KeyFingerprint(key);
	Check(
		fingerprint == int64(-3972359982579920590LL),
		"chat key fingerprint vector");
	Check(
		KeyHashForVisualization(key) == FromHex(
			"4916d6bdb7f78e6803698cab32d1586e"
			"40aff2e9d2d8922e47afd4648e6967497158785f"),
		"chat key visualization vector");

	const auto message = FromHex("01020304");
	const auto creatorPayload = FromHex(
		"32d1586ea457dfc895395cef1d1d8917f96b7aa2a7ca6f91"
		"05bb4fea1092b6953b520099093e2996d7b73757688f9a9f"
		"ad1a0e0d384d4882");
	const auto acceptorPayload = FromHex(
		"32d1586ea457dfc8a14571f54e4552db5a0381d2eea5025c"
		"beb542247dd840db118de07fa10f8eeea9a95c00209d7c46"
		"49870bbe80a2b03c");
	const auto creatorDecrypted = DecryptPayload(
		key,
		fingerprint,
		true,
		creatorPayload);
	const auto acceptorDecrypted = DecryptPayload(
		key,
		fingerprint,
		false,
		acceptorPayload);
	Check(
		creatorDecrypted && *creatorDecrypted == message,
		"creator KDF and AES-IGE vector");
	Check(
		acceptorDecrypted && *acceptorDecrypted == message,
		"acceptor KDF and AES-IGE vector");
	Check(
		!DecryptPayload(key, fingerprint, false, creatorPayload),
		"sender role selects a distinct KDF window");

	auto tampered = creatorPayload;
	tampered.back() = bytes::type(
		gsl::to_integer<unsigned char>(tampered.back()) ^ 1);
	Check(
		!DecryptPayload(key, fingerprint, true, tampered),
		"payload tampering is rejected");

	for (const auto creator : { false, true }) {
		const auto encrypted = EncryptPayload(
			key,
			fingerprint,
			creator,
			message);
		const auto decrypted = DecryptPayload(
			key,
			fingerprint,
			creator,
			encrypted);
		Check(
			PayloadKeyFingerprint(encrypted) == fingerprint,
			creator
				? "creator payload carries its fingerprint"
				: "acceptor payload carries its fingerprint");
		Check(
			decrypted && *decrypted == message,
			creator
				? "creator payload round trip"
				: "acceptor payload round trip");
	}

	const auto fileKey = FileKey{
		.key = bytes::vector(kFileKeySize, bytes::type(0x11)),
		.iv = bytes::vector(kFileIvSize, bytes::type(0x22)),
	};
	Check(
		FileKeyFingerprint(fileKey) == 81791387,
		"file key fingerprint vector");
	const auto filePlain = Sequence(37);
	const auto fileEncrypted = EncryptFile(fileKey, filePlain);
	const auto fileDecrypted = DecryptFile(
		fileKey,
		fileEncrypted,
		int64(filePlain.size()));
	Check(
		fileDecrypted && *fileDecrypted == filePlain,
		"file encryption round trip");
	Check(
		!DecryptFile(fileKey, fileEncrypted, int64(fileEncrypted.size()) + 1),
		"file size larger than the envelope is rejected");

	auto history = AyuSecret::HistoryRecord{
		.id = MsgId(7),
		.randomId = 77,
		.out = true,
		.text = TextWithEntities{ u"queued file"_q },
		.media = QByteArray("encrypted media key"),
		.silent = true,
	};
	const auto historyBytes = AyuSecret::SerializeHistory({ history });
	const auto pendingBytes = historyBytes.value_or(QByteArray());
	const auto restoredHistory = historyBytes
		? AyuSecret::DeserializeHistory(*historyBytes)
		: std::nullopt;
	Check(restoredHistory && restoredHistory->size() == 1
		&& restoredHistory->front().silent
		&& restoredHistory->front().media == history.media,
		"silent media history survives restart");

	auto state = AyuSecret::ChatState();
	state.chatId = 19;
	state.unacked.push_back({
		.outSeqNo = 3,
		.randomId = 88,
		.serialized = QByteArray("encrypted layer"),
		.file = AyuSecret::OutgoingFile{
			.kind = AyuSecret::OutgoingFile::Kind::Uploaded,
			.id = std::numeric_limits<int64>::min() + 17,
			.parts = 2,
			.fingerprint = 42,
		},
		.pendingRecord = pendingBytes,
	});
	state.queuedFiles.push_back({
		.randomId = 99,
		.serialized = QByteArray("queued layer"),
		.file = AyuSecret::OutgoingFile{
			.kind = AyuSecret::OutgoingFile::Kind::BigUploaded,
			.id = std::numeric_limits<int64>::min() + 23,
			.parts = 4,
			.fingerprint = 43,
		},
		.record = pendingBytes,
		.silent = true,
	});
	const auto statesBytes = AyuSecret::SerializeChatStates({ state });
	const auto restoredStates = statesBytes
		? AyuSecret::DeserializeChatStates(*statesBytes)
		: std::nullopt;
	Check(restoredStates && restoredStates->size() == 1
		&& restoredStates->front().unacked.size() == 1
		&& restoredStates->front().unacked.front().file
		&& restoredStates->front().unacked.front().file->id
			== std::numeric_limits<int64>::min() + 17
		&& restoredStates->front().unacked.front().pendingRecord
			== pendingBytes,
		"pending file survives restart with a negative file id");
	Check(restoredStates && restoredStates->size() == 1
		&& restoredStates->front().queuedFiles.size() == 1
		&& restoredStates->front().queuedFiles.front().file.id
			== std::numeric_limits<int64>::min() + 23
		&& restoredStates->front().queuedFiles.front().record
			== pendingBytes
		&& restoredStates->front().queuedFiles.front().silent,
		"queued file survives restart");

	std::cout << (TotalChecks - FailedChecks) << "/" << TotalChecks
		<< " checks passed." << std::endl;
	return FailedChecks ? 1 : 0;
}
