// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

#include "ayu/secret/secret_crypto.h"

#include "secret_scheme.h"

class DocumentData;
struct FilePrepareResult;

namespace Main {
class Session;
} // namespace Main

namespace AyuSecret {

struct HistoryRecord;

struct MediaFile {
	Crypto::FileKey key;
	int64 size = 0;
};

// WHY: media is stored as the end-to-end constructor bytes that arrived and
// becomes a cloud `MessageMedia` only when the history item is built.
[[nodiscard]] bool IsStoredMedia(const SecretTLDecryptedMessageMedia &media);
[[nodiscard]] QByteArray SerializeMedia(
	const SecretTLDecryptedMessageMedia &media);
[[nodiscard]] std::optional<SecretTLDecryptedMessageMedia> ParseMedia(
	const QByteArray &serialized);
[[nodiscard]] std::optional<MediaFile> FileFromMedia(
	const SecretTLDecryptedMessageMedia &media);
[[nodiscard]] QString CaptionFromMedia(
	const SecretTLDecryptedMessageMedia &media);

[[nodiscard]] bool IsSecretFile(not_null<Main::Session*> session, uint64 id);
[[nodiscard]] std::optional<QByteArray> DecryptSecretFile(
	not_null<Main::Session*> session,
	uint64 id,
	const QByteArray &encrypted);

[[nodiscard]] MTPMessageMedia MediaFromRecord(
	not_null<Main::Session*> session,
	const HistoryRecord &record);

[[nodiscard]] std::optional<SecretTLDecryptedMessageMedia> StickerMedia(
	not_null<DocumentData*> document);
[[nodiscard]] std::optional<SecretTLDecryptedMessageMedia> PreparedMedia(
	const FilePrepareResult &file,
	const Crypto::FileKey &key,
	int64 size,
	int layer);

} // namespace AyuSecret
