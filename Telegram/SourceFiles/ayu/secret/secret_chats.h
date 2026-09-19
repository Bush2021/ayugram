// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

#include "api/api_common.h"
#include "ayu/secret/secret_chat_state.h"
#include "ayu/secret/secret_dh.h"
#include "ayu/secret/secret_media.h"
#include "base/timer.h"
#include "data/data_types.h"
#include "mtproto/sender.h"

#include "secret_scheme.h"

class SecretChatData;
struct FilePrepareResult;

namespace Main {
class Session;
} // namespace Main

namespace Storage {
struct UploadedMedia;
enum class SharedMediaType : signed char;
} // namespace Storage

namespace AyuSecret {

struct IncomingLayer;
struct QueuedMessage;

class Chats final {
public:
	explicit Chats(not_null<Main::Session*> session);
	~Chats();

	Chats(const Chats &other) = delete;
	Chats &operator=(const Chats &other) = delete;

	[[nodiscard]] Main::Session &session() const {
		return *_session;
	}

	void apply(const MTPUpdate &update);
	void applyDifference(const MTPVector<MTPEncryptedMessage> &messages);
	void applyQts(int32 qts);

	void createWith(
		not_null<UserData*> user,
		Fn<void(SecretChatData*)> done);

	void sendText(
		not_null<SecretChatData*> peer,
		const TextWithEntities &text,
		FullMsgId replyTo,
		bool silent);
	bool sendMedia(
		not_null<SecretChatData*> peer,
		SecretTLDecryptedMessageMedia &&media,
		FullMsgId replyTo,
		bool silent);
	void sendPrepared(
		const std::shared_ptr<FilePrepareResult> &file,
		FullMsgId placeholder);
	void sendDocumentCopy(
		const Api::SendAction &action,
		not_null<DocumentData*> document);
	void readInbox(
		not_null<SecretChatData*> peer,
		MsgId tillId,
		bool ignoreGhost);
	[[nodiscard]] const MediaFile *file(uint64 id) const;
	void sendTyping(not_null<SecretChatData*> peer);
	void reportSpam(not_null<SecretChatData*> peer);
	void deleteMessages(
		not_null<SecretChatData*> peer,
		const std::vector<MsgId> &ids);
	void clearHistory(not_null<SecretChatData*> peer);
	void deleteChat(not_null<SecretChatData*> peer, bool revoke);
	void setTtl(not_null<SecretChatData*> peer, TimeId ttl);

	[[nodiscard]] SecretChatData *chat(SecretChatId id) const;
	[[nodiscard]] ChatState *state(not_null<SecretChatData*> peer);
	[[nodiscard]] MessageIdsList search(
		not_null<SecretChatData*> peer,
		const QString &query) const;
	[[nodiscard]] std::vector<MsgId> sharedMedia(
		not_null<SecretChatData*> peer,
		Storage::SharedMediaType type) const;

	[[nodiscard]] rpl::producer<not_null<SecretChatData*>> chatAdded() const;

	void saveStates();
	void flush();

private:
	struct Chat {
		ChatState state;
		not_null<SecretChatData*> peer;
		std::vector<HistoryRecord> records;
		base::flat_map<uint64, MsgId> byRandomId;
		std::vector<std::unique_ptr<QueuedMessage>> queue;
		bytes::vector answerGPow;
		int64 answerFingerprint = 0;
		uint64 commitRandomId = 0;
		TimeId readDateSent = 0;
		mtpRequestId readRequestId = 0;
		mtpRequestId typingRequestId = 0;
		bool historyChanged = false;
		bool accepting = false;
		bool sending = false;
	};

	struct Upload {
		SecretChatId chatId;
		SecretTLDecryptedMessageMedia media;
		TextWithEntities caption;
		MsgId replyToLocalId = 0;
		uint64 mediaId = 0;
		uint64 groupedId = 0;
		int32 fingerprint = 0;
		bool photo = false;
		bool silent = false;
	};

	[[nodiscard]] Chat *lookup(SecretChatId id) const;
	[[nodiscard]] not_null<Chat*> emplace(ChatState &&state);
	[[nodiscard]] not_null<Chat*> applyFields(
		SecretChatId id,
		int64 accessHash,
		UserId adminId,
		UserId participantId,
		TimeId date);

	void materialize(not_null<Chat*> chat);
	void setChatState(not_null<Chat*> chat, SecretChatData::State state);
	void acceptRequested(not_null<Chat*> chat, bytes::vector gA);
	void acceptWithConfig(
		not_null<Chat*> chat,
		bytes::const_span gA,
		const Crypto::DhConfig &config);
	void createWithConfig(
		not_null<UserData*> user,
		const Crypto::DhConfig &config,
		Fn<void(SecretChatData*)> done);
	void finishCreator(not_null<Chat*> chat, bool allowConfigRequest);
	void applyKey(
		not_null<Chat*> chat,
		bytes::vector &&key,
		int64 keyFingerprint);
	void sendNotifyLayer(not_null<Chat*> chat);
	void enqueue(
		not_null<Chat*> chat,
		std::unique_ptr<QueuedMessage> message);
	void sendNext(not_null<Chat*> chat);
	void appendRecord(not_null<Chat*> chat, HistoryRecord &&record);
	void startUpload(
		std::shared_ptr<FilePrepareResult> file,
		FullMsgId placeholder,
		Crypto::FileKey key,
		QByteArray encrypted,
		int64 size);
	void uploadReady(const Storage::UploadedMedia &data);
	void uploadProgress(FullMsgId id, int64 offset, int64 size);
	void uploadFailed(FullMsgId id);
	void adoptPlaceholderMedia(FullMsgId id, const HistoryRecord &record);
	void removePlaceholder(FullMsgId id);
	void checkDocumentCopies();
	void sendFileCopy(const Api::SendAction &action, const QString &path);
	void clearQueue(not_null<Chat*> chat);
	void registerFile(const HistoryRecord &record);
	void unregisterFile(const HistoryRecord &record);
	void closeOnError(not_null<Chat*> chat, const QString &reason);
	void sendDiscard(not_null<Chat*> chat, bool deleteHistory);
	void forgetChat(not_null<Chat*> chat);
	void raiseLayer(not_null<Chat*> chat, int layer);
	void receive(
		not_null<Chat*> chat,
		IncomingLayer &&layer,
		QByteArray &&serialized,
		TimeId date);
	void accept(
		not_null<Chat*> chat,
		const IncomingLayer &layer,
		TimeId date,
		bool replay);
	void drainHeld(not_null<Chat*> chat);
	void requestResend(not_null<Chat*> chat);
	void answerResend(not_null<Chat*> chat, int32 startSeqNo, int32 endSeqNo);
	void resendUnacked(not_null<Chat*> chat, const UnackedMessage &message);
	void answerKeyRequest(
		not_null<Chat*> chat,
		int64 exchangeId,
		bytes::vector &&gA);
	void commitKey(not_null<Chat*> chat, int64 exchangeId, int64 fingerprint);
	void countMessage(not_null<Chat*> chat);
	void requestKey(not_null<Chat*> chat);
	void acceptKey(
		not_null<Chat*> chat,
		int64 exchangeId,
		bytes::const_span gB,
		int64 fingerprint);
	void abortKey(not_null<Chat*> chat, int64 exchangeId);
	void switchToNewKey(not_null<Chat*> chat);
	void ackOutgoingUpTo(not_null<Chat*> chat, int32 hisInSeqNo);
	void applyMessage(
		not_null<Chat*> chat,
		const IncomingLayer &layer,
		TimeId date,
		bool replay);
	void applyAction(
		not_null<Chat*> chat,
		uint64 randomId,
		TimeId date,
		const SecretTLDecryptedMessageAction &action,
		bool replay);
	void addService(
		not_null<Chat*> chat,
		uint64 randomId,
		TimeId date,
		ServiceKind kind,
		QByteArray &&payload = QByteArray());
	void removeRecords(not_null<Chat*> chat, const std::vector<MsgId> &ids);
	void dropRecords(not_null<Chat*> chat, const std::vector<MsgId> &ids);
	void updateUnreadCount(not_null<Chat*> chat);
	void checkTtl();
	void sendService(
		not_null<Chat*> chat,
		const SecretTLDecryptedMessageAction &action);
	void receiveText(
		not_null<Chat*> chat,
		uint64 randomId,
		TimeId date,
		int32 ttl,
		TextWithEntities &&text,
		uint64 replyToRandomId,
		const SecretTLDecryptedMessageMedia *media,
		const std::optional<EncryptedFileInfo> &file,
		uint64 groupedId = 0);
	void setRecordDate(not_null<Chat*> chat, uint64 randomId, TimeId date);
	void markSent(not_null<Chat*> chat, uint64 randomId);
	void requeueServices(not_null<Chat*> chat);
	void requeueUnsent(not_null<Chat*> chat);
	[[nodiscard]] bytes::vector takePendingSecret(int32 randomId);
	[[nodiscard]] bytes::vector takePendingKey(SecretChatId id);

	void handleEncryption(const MTPEncryptedChat &chat);
	void handleNewMessage(const MTPEncryptedMessage &message);
	void handleTyping(SecretChatId id);
	void handleMessagesRead(SecretChatId id, TimeId maxDate);

	void readStates();
	void writeStates();
	void readHistory(not_null<Chat*> chat);
	void applyReadState(not_null<Chat*> chat);
	void saveHistory(not_null<Chat*> chat);
	void writeHistories();
	void sendReceivedQueue();

	const not_null<Main::Session*> _session;
	MTP::Sender _api;
	DhConfigCache _dh;

	base::flat_map<SecretChatId, std::unique_ptr<Chat>> _chats;
	base::flat_map<int32, bytes::vector> _pendingSecrets;
	base::flat_map<SecretChatId, bytes::vector> _pendingKeys;
	base::flat_map<uint64, MediaFile> _files;
	base::flat_map<FullMsgId, Upload> _uploads;
	base::flat_map<
		not_null<DocumentData*>,
		std::vector<Api::SendAction>> _documentCopies;
	rpl::event_stream<not_null<SecretChatData*>> _chatAdded;

	base::Timer _saveTimer;
	base::Timer _historyTimer;
	base::Timer _receivedQueueTimer;
	base::Timer _ttlTimer;
	int32 _pendingQts = 0;
	bool _ackNeeded = false;

	rpl::lifetime _lifetime;

};

} // namespace AyuSecret
