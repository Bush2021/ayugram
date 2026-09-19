// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/secret_crypto.h"

#include "base/openssl_help.h"
#include "core/utils.h"
#include "mtproto/mtproto_auth_key.h"
#include "mtproto/mtproto_dh_utils.h"

#include <limits>

namespace AyuSecret::Crypto {
namespace {

constexpr auto kLengthPrefixSize = 4;
constexpr auto kAcceptorShift = 8;
constexpr auto kKdfWindowSize = 36;
constexpr auto kKdfWindowSecondOffset = 40;
constexpr auto kMsgKeyWindowOffset = 88;
constexpr auto kMsgKeyWindowSize = 32;
constexpr auto kMd5Size = 16;
constexpr auto kHeaderSize = kFingerprintSize + kMsgKeySize;
constexpr auto kMaxSingleShotSize = int64(
	std::numeric_limits<uint32>::max()) - kBlockSize;

struct AesKeyIv {
	bytes::vector key;
	bytes::vector iv;
};

[[nodiscard]] int64 ReadInt64LE(bytes::const_span from) {
	Expects(from.size() >= std::size_t(kFingerprintSize));

	auto result = uint64(0);
	for (auto i = 0; i != kFingerprintSize; ++i) {
		result |= gsl::to_integer<uint64>(from[i]) << (8 * i);
	}
	return int64(result);
}

void WriteInt64LE(bytes::span to, int64 value) {
	Expects(to.size() >= std::size_t(kFingerprintSize));

	const auto raw = uint64(value);
	for (auto i = 0; i != kFingerprintSize; ++i) {
		to[i] = bytes::type((raw >> (8 * i)) & 0xFFU);
	}
}

[[nodiscard]] int32 ReadInt32LE(bytes::const_span from) {
	Expects(from.size() >= std::size_t(kLengthPrefixSize));

	auto result = uint32(0);
	for (auto i = 0; i != kLengthPrefixSize; ++i) {
		result |= gsl::to_integer<uint32>(from[i]) << (8 * i);
	}
	return int32(result);
}

void WriteInt32LE(bytes::span to, int32 value) {
	Expects(to.size() >= std::size_t(kLengthPrefixSize));

	const auto raw = uint32(value);
	for (auto i = 0; i != kLengthPrefixSize; ++i) {
		to[i] = bytes::type((raw >> (8 * i)) & 0xFFU);
	}
}

[[nodiscard]] bool ConstantTimeEqual(
		bytes::const_span a,
		bytes::const_span b) {
	return (a.size() == b.size())
		&& (CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0);
}

[[nodiscard]] bytes::vector PadLeftTo(bytes::vector value, int size) {
	if (int(value.size()) > size) {
		return {};
	} else if (int(value.size()) == size) {
		return value;
	}
	auto result = bytes::vector(size);
	bytes::copy(
		bytes::make_span(result).subspan(size - value.size()),
		bytes::make_span(value));
	return result;
}

[[nodiscard]] bytes::vector ModExpPadded(
		const openssl::BigNum &base,
		bytes::const_span power,
		const openssl::BigNum &prime) {
	const auto result = openssl::BigNum::ModExp(
		base,
		openssl::BigNum(power),
		prime);
	if (result.failed() || result.isZero()) {
		return {};
	}
	return PadLeftTo(result.getBytes(), kKeySize);
}

[[nodiscard]] bytes::vector ComputeMsgKey(
		bytes::const_span key,
		int x,
		bytes::const_span data) {
	const auto large = openssl::Sha256(
		key.subspan(kMsgKeyWindowOffset + x, kMsgKeyWindowSize),
		data);
	return bytes::make_vector(
		bytes::make_span(large).subspan(8, kMsgKeySize));
}

[[nodiscard]] AesKeyIv ComputeKeyIv(
		bytes::const_span key,
		int x,
		bytes::const_span msgKey) {
	const auto a = openssl::Sha256(
		msgKey,
		key.subspan(x, kKdfWindowSize));
	const auto b = openssl::Sha256(
		key.subspan(kKdfWindowSecondOffset + x, kKdfWindowSize),
		msgKey);
	const auto spanA = bytes::make_span(a);
	const auto spanB = bytes::make_span(b);
	return {
		.key = bytes::concatenate(
			spanA.subspan(0, 8),
			spanB.subspan(8, 16),
			spanA.subspan(24, 8)),
		.iv = bytes::concatenate(
			spanB.subspan(0, 8),
			spanA.subspan(8, 16),
			spanB.subspan(24, 8)),
	};
}

void Wipe(AesKeyIv &value) {
	ZeroAndClear(value.key);
	ZeroAndClear(value.iv);
}

[[nodiscard]] bool GoodFileKey(const FileKey &fileKey) {
	return (int(fileKey.key.size()) == kFileKeySize)
		&& (int(fileKey.iv.size()) == kFileIvSize);
}

[[nodiscard]] bytes::vector FileKeyDigest(const FileKey &fileKey) {
	const auto full = bytes::concatenate(fileKey.key, fileKey.iv);
	auto result = bytes::vector(kMd5Size);
	hashMd5(full.data(), uint32(full.size()), result.data());
	return result;
}

} // namespace

bool ValidateDhConfig(bytes::const_span p, int g) {
	return (int(p.size()) == kKeySize) && MTP::IsPrimeAndGood(p, g);
}

bytes::vector GenerateDhSecret(bytes::const_span serverRandom) {
	if (int(serverRandom.size()) != kDhRandomSize) {
		return {};
	}
	auto result = bytes::vector(kDhRandomSize);
	bytes::set_random(bytes::make_span(result));
	for (auto i = 0; i != kDhRandomSize; ++i) {
		result[i] ^= serverRandom[i];
	}
	return result;
}

bytes::vector ComputeGPow(bytes::const_span p, int g, bytes::const_span own) {
	if (int(p.size()) != kKeySize || g <= 0 || own.empty()) {
		return {};
	}
	return ModExpPadded(
		openssl::BigNum(static_cast<unsigned int>(g)),
		own,
		openssl::BigNum(p));
}

bool ValidateGPow(bytes::const_span p, bytes::const_span gPow) {
	if (int(p.size()) != kKeySize
		|| gPow.empty()
		|| int(gPow.size()) > kKeySize) {
		return false;
	}
	return MTP::IsGoodModExpFirst(openssl::BigNum(gPow), openssl::BigNum(p));
}

bytes::vector ComputeSharedKey(
		bytes::const_span p,
		bytes::const_span gOther,
		bytes::const_span own) {
	if (!ValidateGPow(p, gOther) || own.empty()) {
		return {};
	}
	return ModExpPadded(
		openssl::BigNum(gOther),
		own,
		openssl::BigNum(p));
}

int64 KeyFingerprint(bytes::const_span key) {
	if (int(key.size()) != kKeySize) {
		return 0;
	}
	const auto hash = openssl::Sha1(key);
	return ReadInt64LE(bytes::make_span(hash).subspan(12, kFingerprintSize));
}

bytes::vector KeyHashForVisualization(bytes::const_span key) {
	if (int(key.size()) != kKeySize) {
		return {};
	}
	const auto sha1 = openssl::Sha1(key);
	const auto sha256 = openssl::Sha256(key);
	auto result = bytes::concatenate(
		bytes::make_span(sha1).subspan(0, 16),
		bytes::make_span(sha256).subspan(0, 20));

	Ensures(int(result.size()) == kKeyHashSize);
	return result;
}

bytes::vector EncryptPayload(
		bytes::const_span key,
		int64 keyFingerprint,
		bool senderIsCreator,
		bytes::const_span serialized) {
	constexpr auto kMaxSerialized = std::numeric_limits<int32>::max()
		- kMaxPaddingSize
		- kLengthPrefixSize;
	if (int(key.size()) != kKeySize
		|| serialized.empty()
		|| serialized.size() > std::size_t(kMaxSerialized)) {
		return {};
	}
	const auto x = senderIsCreator ? 0 : kAcceptorShift;
	const auto length = int(serialized.size());
	const auto unpadded = kLengthPrefixSize + length;
	const auto padding = kMinPaddingSize
		+ ((kBlockSize - ((unpadded + kMinPaddingSize) % kBlockSize))
			% kBlockSize);

	auto data = bytes::vector(unpadded + padding);
	const auto dataSpan = bytes::make_span(data);
	WriteInt32LE(dataSpan, length);
	bytes::copy(dataSpan.subspan(kLengthPrefixSize), serialized);
	bytes::set_random(dataSpan.subspan(unpadded));

	const auto msgKey = ComputeMsgKey(key, x, dataSpan);
	auto keyIv = ComputeKeyIv(key, x, bytes::make_span(msgKey));

	auto result = bytes::vector(kHeaderSize + data.size());
	const auto resultSpan = bytes::make_span(result);
	WriteInt64LE(resultSpan, keyFingerprint);
	bytes::copy(
		resultSpan.subspan(kFingerprintSize),
		bytes::make_span(msgKey));
	MTP::aesIgeEncryptRaw(
		dataSpan.data(),
		resultSpan.subspan(kHeaderSize).data(),
		uint32(data.size()),
		keyIv.key.data(),
		keyIv.iv.data());

	Wipe(keyIv);
	ZeroAndClear(data);
	return result;
}

int64 PayloadKeyFingerprint(bytes::const_span payload) {
	return (int(payload.size()) < kFingerprintSize)
		? 0
		: ReadInt64LE(payload);
}

std::optional<bytes::vector> DecryptPayload(
		bytes::const_span key,
		int64 keyFingerprint,
		bool senderIsCreator,
		bytes::const_span payload) {
	if (int(key.size()) != kKeySize
		|| int(payload.size()) < kHeaderSize + kBlockSize
		|| ((int(payload.size()) - kHeaderSize) % kBlockSize) != 0
		|| ReadInt64LE(payload) != keyFingerprint) {
		return std::nullopt;
	}
	const auto x = senderIsCreator ? 0 : kAcceptorShift;
	const auto msgKey = payload.subspan(kFingerprintSize, kMsgKeySize);
	const auto encrypted = payload.subspan(kHeaderSize);

	auto keyIv = ComputeKeyIv(key, x, msgKey);
	auto data = bytes::vector(encrypted.size());
	const auto dataSpan = bytes::make_span(data);
	MTP::aesIgeDecryptRaw(
		encrypted.data(),
		dataSpan.data(),
		uint32(encrypted.size()),
		keyIv.key.data(),
		keyIv.iv.data());
	Wipe(keyIv);

	const auto recomputed = ComputeMsgKey(key, x, dataSpan);
	if (!ConstantTimeEqual(bytes::make_span(recomputed), msgKey)) {
		ZeroAndClear(data);
		return std::nullopt;
	}
	const auto length = ReadInt32LE(dataSpan);
	const auto available = int(data.size()) - kLengthPrefixSize;
	if (length <= 0 || length > available) {
		ZeroAndClear(data);
		return std::nullopt;
	}
	const auto padding = available - length;
	if (padding < kMinPaddingSize || padding > kMaxPaddingSize) {
		ZeroAndClear(data);
		return std::nullopt;
	}
	auto result = bytes::make_vector(
		dataSpan.subspan(kLengthPrefixSize, length));
	ZeroAndClear(data);
	return result;
}

FileKey GenerateFileKey() {
	auto result = FileKey{
		.key = bytes::vector(kFileKeySize),
		.iv = bytes::vector(kFileIvSize),
	};
	bytes::set_random(bytes::make_span(result.key));
	bytes::set_random(bytes::make_span(result.iv));
	return result;
}

int32 FileKeyFingerprint(const FileKey &fileKey) {
	if (!GoodFileKey(fileKey)) {
		return 0;
	}
	const auto digest = FileKeyDigest(fileKey);
	const auto span = bytes::make_span(digest);
	const auto low = uint32(ReadInt32LE(span));
	const auto high = uint32(ReadInt32LE(span.subspan(kLengthPrefixSize)));
	return int32(low ^ high);
}

bytes::vector EncryptFile(const FileKey &fileKey, bytes::const_span data) {
	const auto size = int64(data.size());
	if (!GoodFileKey(fileKey) || size > kMaxSingleShotSize) {
		return {};
	}
	const auto padding = int((kBlockSize - (size % kBlockSize)) % kBlockSize);
	auto padded = bytes::vector(data.size() + padding);
	if (padded.empty()) {
		return padded;
	}
	const auto paddedSpan = bytes::make_span(padded);
	bytes::copy(paddedSpan, data);
	if (padding > 0) {
		bytes::set_random(paddedSpan.subspan(data.size()));
	}
	auto result = bytes::vector(padded.size());
	MTP::aesIgeEncryptRaw(
		paddedSpan.data(),
		bytes::make_span(result).data(),
		uint32(padded.size()),
		fileKey.key.data(),
		fileKey.iv.data());
	ZeroAndClear(padded);
	return result;
}

std::optional<bytes::vector> DecryptFile(
		const FileKey &fileKey,
		bytes::const_span encrypted,
		int64 originalSize) {
	const auto size = int64(encrypted.size());
	if (!GoodFileKey(fileKey)
		|| size > kMaxSingleShotSize
		|| (size % kBlockSize) != 0
		|| originalSize < 0
		|| originalSize > size) {
		return std::nullopt;
	}
	auto result = bytes::vector(encrypted.size());
	if (result.empty()) {
		return result;
	}
	const auto resultSpan = bytes::make_span(result);
	MTP::aesIgeDecryptRaw(
		encrypted.data(),
		resultSpan.data(),
		uint32(encrypted.size()),
		fileKey.key.data(),
		fileKey.iv.data());
	result.resize(std::size_t(originalSize));
	return result;
}

void ZeroAndClear(bytes::vector &value) {
	if (!value.empty()) {
		bytes::set_random(bytes::make_span(value));
	}
	value.clear();
	value.shrink_to_fit();
}

void ZeroAndClear(FileKey &value) {
	ZeroAndClear(value.key);
	ZeroAndClear(value.iv);
}

} // namespace AyuSecret::Crypto
