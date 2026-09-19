// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

#include "base/bytes.h"

#include <optional>

// MTProto 2.0 end-to-end primitives for secret chats.
//
// Key derivation, slice by slice. The shared key is 256 bytes; `X` selects
// which half of it a side uses, and is 0 when the *sender* of the message is
// the chat creator and 8 when the sender is the acceptor. The same message is
// therefore encrypted by one side and decrypted by the other with the same
// `X`, taken from the sender's role, not from "me" and "them". The message
// key is a hash of the payload keyed by a fixed 32-byte window of the shared
// key, `key[88 + X : 120 + X]`; only the middle 16 bytes of that hash,
// `[8:24]`, travel on the wire, so the value is both an integrity tag and the
// per-message salt of the AES key. The two derivation hashes then mix the
// message key with two further 36-byte windows in opposite order,
// `sha256_a = SHA256(msg_key || key[X : 36 + X])` and
// `sha256_b = SHA256(key[40 + X : 76 + X] || msg_key)`, and the AES-256 key
// and the 32-byte IGE IV are woven from alternating thirds of those two
// hashes, `aes_key = a[0:8] || b[8:24] || a[24:32]` and
// `aes_iv = b[0:8] || a[8:24] || b[24:32]`. Because the ordering of `a` and
// `b` is swapped between key and IV, and because the windows for `X = 0` and
// `X = 8` overlap only partially, an implementation that gets one offset
// wrong still produces plausible-looking output and fails only against a real
// peer -- which is why the vectors below are recorded.
//
// Test vectors, computed with Python 3.14 hashlib (commands at the end of
// this block). The chat key is the 256 bytes 0x00..0xFF, `bytes(range(256))`.
//
//   SHA1(key)   = 4916d6bdb7f78e6803698cab32d1586ea457dfc8
//   SHA256(key) = 40aff2e9d2d8922e47afd4648e69674971
//                 58785fbd1da870e7110266bf944880
//
//   KeyFingerprint(key)
//     = -3972359982579920590 = 0xC8DF57A46E58D132 (int64, little-endian
//       read of SHA1(key)[12:20])
//
//   KeyHashForVisualization(key), 36 bytes
//     = 4916d6bdb7f78e6803698cab32d1586e
//       40aff2e9d2d8922e47afd4648e6967497158785f
//
// With that same key and msg_key = the 16 bytes 0xAB:
//
//   X = 0: aes_key = 6be28b46efd2682463613a0a6921e28c
//                    e3e1d0d2815a126d06dd4ee299bc3344
//          aes_iv  = 33ec57d01b25fa207f472aa956a7041e
//                    731fcb2292895ab9d2fc90026e672745
//   X = 8: aes_key = 915744be728963e0dba0c2196899caa8
//                    20b91848e73a9fd5b49c3be9f56c392d
//          aes_iv  = 007158ffd52f57ac506f4e6c67956a66
//                    d073897f5ce5c685be28e1970397cccd
//
// File key: key = the 32 bytes 0x11, iv = the 32 bytes 0x22.
//
//   MD5(key || iv)     = 2acb1096b1c2f0923ce2cb722828cc1d
//   FileKeyFingerprint = 81791387 = 0x04E0099B (int32)
//
// python -c "import hashlib; k=bytes(range(256)); s1=hashlib.sha1(k).digest();
//   s2=hashlib.sha256(k).digest(); print(s1.hex(), s2.hex());
//   print(int.from_bytes(s1[12:20],'little',signed=True));
//   print((s1[0:16]+s2[0:20]).hex())"
// python -c "import hashlib; k=bytes(range(256)); m=bytes([0xAB]*16)
//   ; [print(X, hashlib.sha256(m+k[X:X+36]).digest().hex(),
//   hashlib.sha256(k[40+X:40+X+36]+m).digest().hex()) for X in (0,8)]"
// python -c "import hashlib; m=hashlib.md5(bytes([0x11]*32)+bytes([0x22]*32))
//   .digest(); print(m.hex());
//   a=int.from_bytes(m[0:4],'little',signed=True);
//   b=int.from_bytes(m[4:8],'little',signed=True);
//   print(((a^b)+2**31)%2**32-2**31)"

namespace AyuSecret::Crypto {

inline constexpr auto kKeySize = 256;
inline constexpr auto kKeyHashSize = 36;
inline constexpr auto kDhRandomSize = 256;
inline constexpr auto kMsgKeySize = 16;
inline constexpr auto kFingerprintSize = 8;
inline constexpr auto kFileKeySize = 32;
inline constexpr auto kFileIvSize = 32;
inline constexpr auto kBlockSize = 16;
inline constexpr auto kMinPaddingSize = 12;
inline constexpr auto kMaxPaddingSize = 1024;

struct DhConfig {
	bytes::vector p;
	bytes::vector random;
	int g = 0;
	int version = 0;
};

// `p` and `g` as they came from `messages.dhConfig`. Rejects anything that
// `MTP::IsPrimeAndGood` does not accept, so a hostile server cannot talk us
// into a weak group.
[[nodiscard]] bool ValidateDhConfig(bytes::const_span p, int g);

// 256 bytes of local randomness mixed with the server's `random` by XOR, so
// neither side alone decides the secret exponent. Empty when `serverRandom`
// is not exactly `kDhRandomSize` bytes.
[[nodiscard]] bytes::vector GenerateDhSecret(bytes::const_span serverRandom);

// `g ^ own mod p`, left-zero-padded to `kKeySize`. Empty on failure.
[[nodiscard]] bytes::vector ComputeGPow(
	bytes::const_span p,
	int g,
	bytes::const_span own);

// `MTP::IsGoodModExpFirst` on the peer's (or our own) `g_a` / `g_b`.
[[nodiscard]] bool ValidateGPow(bytes::const_span p, bytes::const_span gPow);

// `gOther ^ own mod p`, left-zero-padded to `kKeySize`. Revalidates
// `gOther` and returns empty when it does not pass.
[[nodiscard]] bytes::vector ComputeSharedKey(
	bytes::const_span p,
	bytes::const_span gOther,
	bytes::const_span own);

// int64, little-endian, from bytes 12..19 of `SHA1(key)`.
[[nodiscard]] int64 KeyFingerprint(bytes::const_span key);

// `SHA1(key)[0:16] || SHA256(key)[0:20]`, 36 bytes, the input of the
// 12 by 12 key visualization. Empty when `key` has the wrong size.
[[nodiscard]] bytes::vector KeyHashForVisualization(bytes::const_span key);

// `serialized` is the already boxed TL body of `decryptedMessageLayer`.
// Returns `key_fingerprint || msg_key || AES256_IGE(data)`, or empty when
// the inputs are malformed. `senderIsCreator` is the role of the side that
// produces the message, which for this call is always us.
[[nodiscard]] bytes::vector EncryptPayload(
	bytes::const_span key,
	int64 keyFingerprint,
	bool senderIsCreator,
	bytes::const_span serialized);

[[nodiscard]] int64 PayloadKeyFingerprint(bytes::const_span payload);

// The inverse. `senderIsCreator` is the role of the peer that sent the
// payload. Every check failure returns `std::nullopt`; nothing throws and
// nothing is logged.
[[nodiscard]] std::optional<bytes::vector> DecryptPayload(
	bytes::const_span key,
	int64 keyFingerprint,
	bool senderIsCreator,
	bytes::const_span payload);

struct FileKey {
	bytes::vector key;
	bytes::vector iv;
};

[[nodiscard]] FileKey GenerateFileKey();

// int32(MD5(key || iv)[0:4]) XOR int32(MD5(key || iv)[4:8]), both read
// little-endian. Zero when the key is malformed.
[[nodiscard]] int32 FileKeyFingerprint(const FileKey &fileKey);

// Pads with random bytes up to a multiple of 16, then AES-256-IGE. Empty on
// a malformed key.
[[nodiscard]] bytes::vector EncryptFile(
	const FileKey &fileKey,
	bytes::const_span data);

// Decrypts and strips the padding down to `originalSize`, the `size` the
// peer declared in the decrypted media. `std::nullopt` when the sizes do not
// agree.
[[nodiscard]] std::optional<bytes::vector> DecryptFile(
	const FileKey &fileKey,
	bytes::const_span encrypted,
	int64 originalSize);

// Overwrites the storage with random bytes before releasing it, so a key
// never survives in freed heap. Random rather than zeroes because the
// compiler cannot fold a random fill away.
void ZeroAndClear(bytes::vector &value);
void ZeroAndClear(FileKey &value);

} // namespace AyuSecret::Crypto
