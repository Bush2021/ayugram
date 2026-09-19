// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

#include "ayu/secret/data_secret_chat.h"
#include "base/bytes.h"
#include "data/data_peer_id.h"

#include <optional>

// Persistent form of a secret chat. Two blobs, both produced here and handed
// to `Storage::Account` as opaque byte arrays: one carries every chat's
// protocol state, one carries a single chat's message history. Neither blob
// is ever written unencrypted; the account wraps both in the local key the
// same way it wraps drafts.
//
// Both blobs start with a version and a count, every field is written in
// declaration order, and readers stop at the first sign of trouble and
// return nothing. A blob that fails to read is treated as absent, never as a
// reason to assert: it is file content, and file content is not trusted.

namespace AyuSecret {

inline constexpr auto kMaxMessageTtl = int32(7 * 86400);

struct PfsState {
	enum class Stage : uchar {
		None,
		Requested,
		Accepted,
	};

	Stage stage = Stage::None;
	int64 exchangeId = 0;
	bytes::vector a;
	bytes::vector otherKey;
	int64 otherFingerprint = 0;
};

struct EncryptedFileInfo {
	int64 id = 0;
	int64 accessHash = 0;
	int64 size = 0;
	int dcId = 0;
	int32 keyFingerprint = 0;
};

struct OutgoingFile {
	enum class Kind : int32 {
		Uploaded = 1,
		BigUploaded = 2,
		Existing = 3,
	};
	Kind kind = Kind::Uploaded;
	int64 id = 0;
	int32 parts = 0;
	QString checksum;
	int32 fingerprint = 0;
	int64 accessHash = 0;
};

struct UnackedMessage {
	int32 outSeqNo = 0;
	uint64 randomId = 0;
	QByteArray serialized;
	std::optional<OutgoingFile> file;
	QByteArray pendingRecord;
};

struct QueuedFile {
	uint64 randomId = 0;
	QByteArray serialized;
	OutgoingFile file;
	QByteArray record;
	bool silent = false;
};

struct PendingService {
	uint64 randomId = 0;
	QByteArray serialized;
};

struct HeldMessage {
	int32 outSeqNo = 0;
	QByteArray decrypted;
	TimeId date = 0;
	std::optional<EncryptedFileInfo> file;
};

struct ChatState {
	int32 chatId = 0;
	int64 accessHash = 0;
	UserId userId;
	bool creator = false;
	SecretChatData::State state = SecretChatData::State::Requested;
	TimeId date = 0;
	int layerHis = SecretChatData::kDefaultLayer;
	bytes::vector key;
	int64 keyFingerprint = 0;
	TimeId keySetDate = 0;
	int messagesSinceKey = 0;
	bytes::vector a;
	int32 ttl = 0;
	int32 inCount = 0;
	int32 outCount = 0;
	MsgId nextMsgId = 1;
	PfsState pfs;
	std::vector<UnackedMessage> unacked;
	std::vector<QueuedFile> queuedFiles;
	std::vector<HeldMessage> held;
	int32 resendRequestedUpTo = 0;
	int32 hisInSeqNo = 0;
	bytes::vector gA;
	std::vector<PendingService> services;
	QByteArray user;
	bytes::vector keyHash;
};

[[nodiscard]] std::optional<QByteArray> SerializeChatStates(
	const std::vector<ChatState> &states);
[[nodiscard]] std::optional<std::vector<ChatState>> DeserializeChatStates(
	const QByteArray &serialized);

enum class ServiceKind : int32 {
	None,
	SetTtl,
	Screenshot,
	FileRejected,
};

struct HistoryRecord {
	MsgId id = 0;
	uint64 randomId = 0;
	bool out = false;
	bool unread = false;
	bool unsent = false;
	TimeId date = 0;
	int32 ttl = 0;
	TimeId destroyAt = 0;
	TextWithEntities text;
	MsgId replyToLocalId = 0;
	ServiceKind serviceKind = ServiceKind::None;
	QByteArray servicePayload;
	QByteArray media;
	std::optional<EncryptedFileInfo> file;
	uint64 groupedId = 0;
	bool mediaUnread = false;
	bool silent = false;
};

[[nodiscard]] std::optional<QByteArray> SerializeHistory(
	const std::vector<HistoryRecord> &records);
[[nodiscard]] std::optional<std::vector<HistoryRecord>> DeserializeHistory(
	const QByteArray &serialized);

} // namespace AyuSecret
