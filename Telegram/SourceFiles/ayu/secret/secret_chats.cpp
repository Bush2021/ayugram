// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/secret_chats.h"

#include "ayu/ayu_settings.h"
#include "ayu/secret/data_secret_chat.h"
#include "ayu/secret/secret_crypto.h"
#include "ayu/secret/secret_inbound.h"
#include "ayu/secret/secret_media.h"
#include "ayu/secret/secret_messages.h"
#include "ayu/secret/secret_outbound.h"
#include "apiwrap.h"
#include "base/random.h"
#include "base/unixtime.h"
#include "core/version.h"
#include "data/notify/data_notify_settings.h"
#include "data/data_document.h"
#include "data/data_document_media.h"
#include "data/data_file_origin.h"
#include "data/data_media_types.h"
#include "data/data_photo.h"
#include "data/data_premium_limits.h"
#include "data/data_send_action.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mtproto/mtproto_response.h"
#include "storage/details/storage_file_utilities.h"
#include "storage/file_upload.h"
#include "storage/localimageloader.h"
#include "storage/serialize_common.h"
#include "storage/serialize_peer.h"
#include "storage/storage_media_prepare.h"
#include "storage/storage_shared_media.h"
#include "storage/storage_account.h"
#include "ui/chat/attach/attach_prepare.h"
#include "ui/item_text_options.h"
#include "ui/toast/toast.h"
#include "styles/style_boxes.h"

#include <algorithm>
#include <limits>

namespace AyuSecret {
namespace {

constexpr auto kSaveDelay = crl::time(1000);
constexpr auto kResendRetryDelay = crl::time(30'000);
constexpr auto kMaxResendCount = 1000;
constexpr auto kMaxHeldMessages = 1000;
constexpr auto kMessagesPerKey = 100;
constexpr auto kKeyLifetime = TimeId(7 * 86400);
constexpr auto kMinPfsLayer = 20;

[[nodiscard]] int32 ClampTtl(int64 ttl) {
	return int32(std::clamp<int64>(ttl, 0, kMaxMessageTtl));
}

[[nodiscard]] TimeId DestroyAt(int32 ttl) {
	return TimeId(std::min<int64>(
		int64(std::numeric_limits<TimeId>::max()),
		int64(base::unixtime::now()) + ClampTtl(ttl)));
}

[[nodiscard]] bool RequiresContentRead(const HistoryRecord &record) {
	const auto media = record.media.isEmpty()
		? std::optional<SecretTLDecryptedMessageMedia>()
		: ParseMedia(record.media);
	return media && MediaRequiresContentRead(*media, record.ttl);
}

[[nodiscard]] QString StateName(SecretChatData::State state) {
	switch (state) {
	case SecretChatData::State::Requested: return u"requested"_q;
	case SecretChatData::State::Waiting: return u"waiting"_q;
	case SecretChatData::State::Ready: return u"ready"_q;
	case SecretChatData::State::Closed: return u"closed"_q;
	}
	return u"unknown"_q;
}

[[nodiscard]] HistoryRecord *FindRecord(
		std::vector<HistoryRecord> &records,
		MsgId id) {
	for (auto &record : records) {
		if (record.id == id) {
			return &record;
		}
	}
	return nullptr;
}

[[nodiscard]] HistoryRecord *FindReplied(
		std::vector<HistoryRecord> &records,
		not_null<PeerData*> peer,
		FullMsgId replyTo) {
	return (replyTo.peer == peer->id && replyTo.msg)
		? FindRecord(records, replyTo.msg)
		: nullptr;
}

[[nodiscard]] uint64 RandomId() {
	auto result = base::RandomValue<uint64>();
	while (!result) {
		result = base::RandomValue<uint64>();
	}
	return result;
}

[[nodiscard]] std::unique_ptr<QueuedMessage> MakeService(
		const SecretTLDecryptedMessageAction &action) {
	const auto randomId = RandomId();
	return std::make_unique<QueuedMessage>(QueuedMessage{
		.message = secret_decryptedMessageService(
			secret_long(randomId),
			action),
		.randomId = randomId,
		.service = true,
	});
}

[[nodiscard]] EncryptedFileInfo FileInfo(const MTPDencryptedFile &data) {
	return EncryptedFileInfo{
		.id = int64(data.vid().v),
		.accessHash = int64(data.vaccess_hash().v),
		.size = int64(data.vsize().v),
		.dcId = data.vdc_id().v,
		.keyFingerprint = data.vkey_fingerprint().v,
	};
}

[[nodiscard]] QByteArray SerializeUser(not_null<UserData*> user) {
	auto stream = Serialize::ByteArrayWriter(
		sizeof(quint32) + Serialize::peerSize(user));
	stream << quint32(AppVersion);
	Serialize::writePeer(stream, user);
	return std::move(stream).result();
}

void RestoreUser(
		not_null<Main::Session*> session,
		const QByteArray &serialized) {
	if (serialized.isEmpty()) {
		return;
	}
	auto stream = Serialize::ByteArrayReader(serialized);
	auto streamAppVersion = quint32();
	stream >> streamAppVersion;
	if (stream.ok()) {
		Serialize::readPeer(session, streamAppVersion, stream);
	}
}

void ClearBytes(QByteArray &value) {
	if (!value.isEmpty()) {
		base::RandomFill(value.data(), value.size());
		value.clear();
		value.squeeze();
	}
}

void ClearKeyMaterial(ChatState &state) {
	Crypto::ZeroAndClear(state.key);
	Crypto::ZeroAndClear(state.keyHash);
	Crypto::ZeroAndClear(state.a);
	Crypto::ZeroAndClear(state.pfs.a);
	Crypto::ZeroAndClear(state.pfs.otherKey);
	for (auto &message : state.unacked) {
		ClearBytes(message.serialized);
		ClearBytes(message.pendingRecord);
	}
	for (auto &file : state.queuedFiles) {
		ClearBytes(file.serialized);
		ClearBytes(file.record);
	}
	for (auto &message : state.held) {
		ClearBytes(message.decrypted);
	}
	for (auto &service : state.services) {
		ClearBytes(service.serialized);
	}
}

} // namespace

Chats::Chats(not_null<Main::Session*> session)
: _session(session)
, _api(&session->mtp())
, _dh(&_api)
, _saveTimer([=] { writeStates(); })
, _historyTimer([=] { writeHistories(); })
, _receivedQueueTimer([=] { sendReceivedQueue(); })
, _resendRetryTimer([=] {
	for (const auto &[id, chat] : _chats) {
		if (!chat->state.held.empty()
			&& !chat->state.resendRequestedUpTo) {
			requestResend(chat.get());
		}
		for (const auto randomId : base::take(chat->pendingFileRetries)) {
			const auto i = ranges::find(
				chat->state.unacked,
				randomId,
				&UnackedMessage::randomId);
			if (i != end(chat->state.unacked)
				&& !i->pendingRecord.isEmpty()
				&& chat->sendingRandomId != randomId) {
				resendUnacked(chat.get(), *i);
			}
		}
	}
})
, _ttlTimer([=] { checkTtl(); }) {
	crl::on_main(_session.get(), [=] {
		readStates();
	});
	_session->uploader().secretReady(
	) | rpl::on_next([=](const Storage::UploadedMedia &data) {
		uploadReady(data);
	}, _lifetime);
	_session->uploader().secretProgress(
	) | rpl::on_next([=](const Storage::UploadSecureProgress &data) {
		uploadProgress(data.fullId, data.offset, data.size);
	}, _lifetime);
	_session->uploader().secretFailed(
	) | rpl::on_next([=](FullMsgId id) {
		uploadFailed(id);
	}, _lifetime);
	_session->downloaderTaskFinished(
	) | rpl::on_next([=] {
		checkDocumentCopies();
	}, _lifetime);
}

Chats::~Chats() {
	flush();
	for (const auto &[id, chat] : _chats) {
		ClearKeyMaterial(chat->state);
	}
	for (auto &[randomId, secret] : _pendingSecrets) {
		Crypto::ZeroAndClear(secret);
	}
	for (auto &[id, key] : _pendingKeys) {
		Crypto::ZeroAndClear(key);
	}
}

void Chats::apply(const MTPUpdate &update) {
	update.match([&](const MTPDupdateNewEncryptedMessage &data) {
		handleNewMessage(data.vmessage());
		applyQts(data.vqts().v);
	}, [&](const MTPDupdateEncryption &data) {
		handleEncryption(data.vchat());
	}, [&](const MTPDupdateEncryptedChatTyping &data) {
		handleTyping(SecretChatIdFromServer(data.vchat_id()));
	}, [&](const MTPDupdateEncryptedMessagesRead &data) {
		handleMessagesRead(
			SecretChatIdFromServer(data.vchat_id()),
			data.vmax_date().v);
	}, [](const auto &) {
		Unexpected("Update type in AyuSecret::Chats::apply.");
	});
}

void Chats::applyDifference(const MTPVector<MTPEncryptedMessage> &messages) {
	if (messages.v.isEmpty()) {
		return;
	}
	DEBUG_LOG(("Secret Info: difference brought %1 encrypted messages."
		).arg(messages.v.size()));
	for (const auto &message : messages.v) {
		handleNewMessage(message);
	}
}

void Chats::applyQts(int32 qts) {
	if (qts > _pendingQts) {
		_pendingQts = qts;
	}
	if (_ackNeeded && _pendingQts && !_receivedQueueTimer.isActive()) {
		_receivedQueueTimer.callOnce(0);
	}
}

void Chats::createWith(
		not_null<UserData*> user,
		Fn<void(SecretChatData*)> done) {
	const auto userId = peerToUser(user->id);
	_dh.request([=](const Crypto::DhConfig *config) {
		const auto loaded = session().data().userLoaded(userId);
		if (!config) {
			DEBUG_LOG(("Secret Error: no DH config, nothing was created."));
			done(nullptr);
			return;
		} else if (!loaded) {
			DEBUG_LOG(("Secret Error: the user is gone, nothing "
				"was created."));
			done(nullptr);
			return;
		}
		createWithConfig(loaded, *config, done);
	});
}

void Chats::sendText(
		not_null<SecretChatData*> peer,
		const TextWithEntities &text,
		FullMsgId replyTo,
		bool silent) {
	const auto chat = lookup(peer->secretChatId());
	if (!chat) {
		DEBUG_LOG(("Secret Error: chat %1 is not known, nothing is sent."
			).arg(peer->serverChatId()));
		return;
	} else if (chat->state.state != SecretChatData::State::Ready
		|| chat->state.key.empty()) {
		DEBUG_LOG(("Secret Error: chat %1 has no key yet, nothing is sent."
			).arg(chat->state.chatId));
		return;
	}
	const auto replied = FindReplied(chat->records, peer, replyTo);
	const auto replyToRandomId = replied ? replied->randomId : uint64(0);
	const auto replyToLocalId = replied ? replied->id : MsgId(0);
	const auto ttl = ClampTtl(chat->state.ttl);

	auto left = text;
	TextUtilities::PrepareForSending(
		left,
		Ui::ItemTextOptions(
			session().data().history(peer),
			session().user()).flags);
	const auto limit = Data::PremiumLimits(_session).messageLengthCurrent();
	auto sending = TextWithEntities();
	while (TextUtilities::CutPart(sending, left, limit)) {
		TextUtilities::Trim(left);
		TextUtilities::Trim(sending);
		const auto randomId = RandomId();
		auto message = MakeTextMessage(
			_session,
			randomId,
			ttl,
			sending,
			replyToRandomId,
			silent);
		appendRecord(chat, HistoryRecord{
			.id = chat->state.nextMsgId++,
			.randomId = randomId,
			.out = true,
			.unread = true,
			.unsent = true,
			.date = base::unixtime::now(),
			.ttl = ttl,
			.text = sending,
			.replyToLocalId = replyToLocalId,
			.silent = silent,
		});
		enqueue(chat, std::make_unique<QueuedMessage>(QueuedMessage{
			.message = std::move(message),
			.randomId = randomId,
			.silent = silent,
		}));
	}
}

bool Chats::sendMedia(
		not_null<SecretChatData*> peer,
		SecretTLDecryptedMessageMedia &&media,
		FullMsgId replyTo,
		bool silent) {
	const auto chat = lookup(peer->secretChatId());
	if (!chat
		|| chat->state.state != SecretChatData::State::Ready
		|| chat->state.key.empty()) {
		return false;
	}
	const auto replied = FindReplied(chat->records, peer, replyTo);
	const auto randomId = RandomId();
	const auto ttl = AdjustedMediaTtl(media, chat->state.ttl);
	const auto mediaUnread = MediaRequiresContentRead(media, ttl);
	appendRecord(chat, HistoryRecord{
		.id = chat->state.nextMsgId++,
		.randomId = randomId,
		.out = true,
		.unread = true,
		.unsent = true,
		.date = base::unixtime::now(),
		.ttl = ttl,
		.replyToLocalId = replied ? replied->id : MsgId(0),
		.media = SerializeMedia(media),
		.mediaUnread = mediaUnread,
		.silent = silent,
	});
	enqueue(chat, std::make_unique<QueuedMessage>(QueuedMessage{
		.message = MakeTextMessage(
			_session,
			randomId,
			ttl,
			TextWithEntities(),
			replied ? replied->randomId : uint64(0),
			silent,
			std::move(media)),
		.randomId = randomId,
		.silent = silent,
	}));
	return true;
}

void Chats::sendDocumentCopy(
		const Api::SendAction &action,
		not_null<DocumentData*> document) {
	if (const auto path = document->filepath(true); !path.isEmpty()) {
		sendFileCopy(action, path);
		return;
	}
	auto &actions = _documentCopies[document];
	actions.push_back(action);
	if (actions.size() == 1) {
		const auto folder = _session->local().tempDirectory();
		QDir().mkpath(folder);
		const auto name = document->filename();
		document->save(
			document->stickerOrGifOrigin(),
			folder
				+ QString::number(document->id)
				+ '_'
				+ (name.isEmpty() ? u"animation.mp4"_q : name));
	}
	checkDocumentCopies();
}

void Chats::checkDocumentCopies() {
	auto finished = std::vector<not_null<DocumentData*>>();
	for (const auto &[document, actions] : _documentCopies) {
		if (!document->loading()) {
			finished.push_back(document);
		}
	}
	for (const auto document : finished) {
		const auto actions = _documentCopies.take(document);
		const auto path = document->filepath(true);
		if (path.isEmpty()) {
			if (!document->cancelled()) {
				Ui::Toast::Show(tr::ayu_SecretChatFileFailed(tr::now));
			}
			continue;
		}
		for (const auto &action : *actions) {
			sendFileCopy(action, path);
		}
	}
}

void Chats::sendFileCopy(
		const Api::SendAction &action,
		const QString &path) {
	auto list = Storage::PrepareMediaList(
		QStringList(path),
		st::sendMediaPreviewSize,
		_session->premium());
	if (list.error != Ui::PreparedList::Error::None) {
		Ui::Toast::Show(tr::ayu_SecretChatFileFailed(tr::now));
		return;
	}
	_session->api().sendFiles(
		std::move(list),
		SendMediaType::Photo,
		nullptr,
		action);
}

void Chats::sendPrepared(
		const std::shared_ptr<FilePrepareResult> &file,
		FullMsgId placeholder) {
	auto &owner = _session->data();
	const auto peer = owner.peer(file->to.peer)->asSecretChat();
	const auto chat = peer ? lookup(peer->secretChatId()) : nullptr;
	if (!chat
		|| chat->state.state != SecretChatData::State::Ready
		|| chat->state.key.empty()) {
		removePlaceholder(placeholder);
		return;
	}
	if (file->type == SendMediaType::Photo) {
		owner.photo(file->id)->uploadingData
			= std::make_unique<Data::UploadState>(file->partssize);
	} else {
		const auto document = owner.document(file->id);
		document->uploadingData = std::make_unique<Data::UploadState>(
			document->size);
		document->uploadingData->preparing = true;
		if (!file->content.isEmpty()) {
			document->setDataAndCache(file->content);
		} else if (!file->filepath.isEmpty()) {
			document->setLocation(Core::FileLocation(file->filepath));
		}
	}
	const auto session = _session;
	crl::async([=] {
		auto plain = QByteArray();
		if (file->type == SendMediaType::Photo) {
			for (const auto &part : file->fileparts) {
				plain.append(part);
			}
		} else if (!file->content.isEmpty()) {
			plain = file->content;
		} else {
			auto f = QFile(file->filepath);
			if (f.open(QIODevice::ReadOnly)) {
				plain = f.readAll();
			}
		}
		auto key = Crypto::GenerateFileKey();
		auto encrypted = QByteArray();
		if (!plain.isEmpty()) {
			const auto result = Crypto::EncryptFile(
				key,
				bytes::make_span(plain));
			encrypted = QByteArray(
				reinterpret_cast<const char*>(result.data()),
				int(result.size()));
		}
		const auto size = int64(plain.size());
		crl::on_main(session.get(), [
			=,
			key = std::move(key),
			encrypted = std::move(encrypted)
		]() mutable {
			session->ayuSecret().startUpload(
				file,
				placeholder,
				std::move(key),
				std::move(encrypted),
				size);
		});
		ClearBytes(plain);
	});
}

void Chats::startUpload(
		std::shared_ptr<FilePrepareResult> file,
		FullMsgId placeholder,
		Crypto::FileKey key,
		QByteArray encrypted,
		int64 size) {
	const auto clearKey = gsl::finally([&] { Crypto::ZeroAndClear(key); });
	auto &owner = _session->data();
	if (!owner.message(placeholder)) {
		return;
	}
	const auto peer = owner.peer(file->to.peer)->asSecretChat();
	const auto chat = peer ? lookup(peer->secretChatId()) : nullptr;
	auto media = (chat && !encrypted.isEmpty())
		? PreparedMedia(*file, key, size, chat->state.layerHis)
		: std::nullopt;
	if (!media) {
		DEBUG_LOG(("Secret Error: could not prepare a file of %1 bytes."
			).arg(size));
		removePlaceholder(placeholder);
		Ui::Toast::Show(tr::ayu_SecretChatFileFailed(tr::now));
		return;
	}
	auto caption = TextWithEntities{
		file->caption.text,
		TextUtilities::ConvertTextTagsToEntities(file->caption.tags),
	};
	TextUtilities::Trim(caption);
	const auto photo = (file->type == SendMediaType::Photo);
	const auto state = photo
		? owner.photo(file->id)->uploadingData.get()
		: owner.document(file->id)->uploadingData.get();
	if (state) {
		state->size = encrypted.size();
		state->offset = 0;
		state->preparing = false;
	}
	const auto id = placeholder;
	const auto album = file->album.lock();
	_uploads.emplace(id, Upload{
		.chatId = peer->secretChatId(),
		.media = std::move(*media),
		.caption = std::move(caption),
		.replyToLocalId = (file->to.replyTo.messageId.peer == peer->id)
			? file->to.replyTo.messageId.msg
			: MsgId(0),
		.mediaId = file->id,
		.groupedId = album ? album->groupId : uint64(0),
		.fingerprint = Crypto::FileKeyFingerprint(key),
		.photo = photo,
		.silent = file->to.options.silent,
	});
	auto upload = MakePreparedFile({
		.id = RandomId(),
		.type = SendMediaType::SecretFile,
	});
	upload->filesize = encrypted.size();
	upload->content = std::move(encrypted);
	_session->uploader().upload(id, upload);
}

void Chats::uploadReady(const Storage::UploadedMedia &data) {
	const auto upload = _uploads.take(data.fullId);
	const auto chat = upload ? lookup(upload->chatId) : nullptr;
	if (!chat || chat->state.state != SecretChatData::State::Ready) {
		removePlaceholder(data.fullId);
		return;
	}
	const auto fingerprint = MTP_int(upload->fingerprint);
	const auto input = data.info.file.match([&](const MTPDinputFile &d) {
		return MTP_inputEncryptedFileUploaded(
			d.vid(),
			d.vparts(),
			d.vmd5_checksum(),
			fingerprint);
	}, [&](const MTPDinputFileBig &d) {
		return MTP_inputEncryptedFileBigUploaded(
			d.vid(),
			d.vparts(),
			fingerprint);
	}, [](const auto &) {
		return MTP_inputEncryptedFileEmpty();
	});
	const auto replied = upload->replyToLocalId
		? FindRecord(chat->records, upload->replyToLocalId)
		: nullptr;
	const auto randomId = RandomId();
	const auto ttl = AdjustedMediaTtl(upload->media, chat->state.ttl);
	const auto mediaUnread = MediaRequiresContentRead(upload->media, ttl);
	enqueue(chat, std::make_unique<QueuedMessage>(QueuedMessage{
		.message = MakeTextMessage(
			_session,
			randomId,
			ttl,
			upload->caption,
			replied ? replied->randomId : uint64(0),
			upload->silent,
			upload->media,
			upload->groupedId),
		.randomId = randomId,
		.silent = upload->silent,
		.file = input,
		.record = HistoryRecord{
			.randomId = randomId,
			.out = true,
			.unread = true,
			.date = base::unixtime::now(),
			.ttl = ttl,
			.text = upload->caption,
			.replyToLocalId = replied ? upload->replyToLocalId : MsgId(0),
			.media = SerializeMedia(upload->media),
			.groupedId = upload->groupedId,
			.mediaUnread = mediaUnread,
			.silent = upload->silent,
		},
		.placeholder = data.fullId,
	}));
}

namespace {

[[nodiscard]] std::optional<OutgoingFile> OutgoingFileFromInput(
		const MTPInputEncryptedFile &input) {
	return input.match([](const MTPDinputEncryptedFileUploaded &data)
			-> std::optional<OutgoingFile> {
		return OutgoingFile{
			.kind = OutgoingFile::Kind::Uploaded,
			.id = int64(data.vid().v),
			.parts = data.vparts().v,
			.checksum = qs(data.vmd5_checksum()),
			.fingerprint = data.vkey_fingerprint().v,
		};
	}, [](const MTPDinputEncryptedFileBigUploaded &data)
			-> std::optional<OutgoingFile> {
		return OutgoingFile{
			.kind = OutgoingFile::Kind::BigUploaded,
			.id = int64(data.vid().v),
			.parts = data.vparts().v,
			.fingerprint = data.vkey_fingerprint().v,
		};
	}, [](const MTPDinputEncryptedFile &data)
			-> std::optional<OutgoingFile> {
		return OutgoingFile{
			.kind = OutgoingFile::Kind::Existing,
			.id = int64(data.vid().v),
			.accessHash = int64(data.vaccess_hash().v),
		};
	}, [](const auto &) -> std::optional<OutgoingFile> {
		return std::nullopt;
	});
}

[[nodiscard]] MTPInputEncryptedFile InputFromOutgoingFile(
		const OutgoingFile &file) {
	switch (file.kind) {
	case OutgoingFile::Kind::Uploaded:
		return MTP_inputEncryptedFileUploaded(
			MTP_long(file.id),
			MTP_int(file.parts),
			MTP_string(file.checksum),
			MTP_int(file.fingerprint));
	case OutgoingFile::Kind::BigUploaded:
		return MTP_inputEncryptedFileBigUploaded(
			MTP_long(file.id),
			MTP_int(file.parts),
			MTP_int(file.fingerprint));
	case OutgoingFile::Kind::Existing:
		return MTP_inputEncryptedFile(
			MTP_long(file.id),
			MTP_long(file.accessHash));
	}
	return MTP_inputEncryptedFileEmpty();
}

} // namespace

void Chats::cancelPlaceholder(FullMsgId placeholder) {
	if (_uploads.take(placeholder)) {
		_session->uploader().cancel(placeholder);
	}
	for (const auto &[id, owned] : _chats) {
		const auto chat = owned.get();
		auto cancelled = std::vector<uint64>();
		chat->queue.erase(ranges::remove_if(chat->queue, [&](const auto &queued) {
			if (!queued || queued->placeholder != placeholder) {
				return false;
			}
			cancelled.push_back(queued->randomId);
			return true;
		}), end(chat->queue));
		if (!cancelled.empty()) {
			chat->state.queuedFiles.erase(ranges::remove_if(
				chat->state.queuedFiles,
				[&](QueuedFile &file) {
					if (!ranges::contains(cancelled, file.randomId)) {
						return false;
					}
					ClearBytes(file.serialized);
					ClearBytes(file.record);
					return true;
				}), end(chat->state.queuedFiles));
			saveStates();
			flush();
			Storage::details::Sync();
			sendNext(chat);
		}
		if (chat->sendingPlaceholder != placeholder) {
			continue;
		}
		const auto randomId = chat->sendingRandomId;
		chat->sending = false;
		chat->sendingRandomId = 0;
		chat->sendingPlaceholder = FullMsgId();
		_api.request(base::take(chat->sendRequestId)).cancel();
		replaceUnacked(chat, randomId);
		saveStates();
		sendNext(chat);
		return;
	}
}

void Chats::uploadProgress(FullMsgId id, int64 offset, int64 size) {
	const auto i = _uploads.find(id);
	if (i == end(_uploads)) {
		return;
	}
	auto &owner = _session->data();
	const auto state = i->second.photo
		? owner.photo(i->second.mediaId)->uploadingData.get()
		: owner.document(i->second.mediaId)->uploadingData.get();
	if (state) {
		state->size = size;
		state->offset = std::min(offset, size);
	}
	if (const auto item = owner.message(id)) {
		owner.requestItemRepaint(item);
	}
}

void Chats::uploadFailed(FullMsgId id) {
	if (!_uploads.take(id)) {
		return;
	}
	// WHY: cancelling from the bubble destroys the item before the uploader
	// reports it, so an item still alive on the next loop means a failure.
	crl::on_main(_session.get(), [=] {
		if (_session->data().message(id)) {
			removePlaceholder(id);
			Ui::Toast::Show(tr::ayu_SecretChatFileFailed(tr::now));
		}
	});
}

void Chats::adoptPlaceholderMedia(
		FullMsgId id,
		const HistoryRecord &record) {
	auto &owner = _session->data();
	const auto item = (id && record.file) ? owner.message(id) : nullptr;
	const auto local = item ? item->media() : nullptr;
	if (!local) {
		return;
	}
	const auto media = MediaFromRecord(_session, record);
	media.match([&](const MTPDmessageMediaPhoto &data) {
		const auto photo = local->photo();
		if (photo && data.vphoto()) {
			owner.photoConvert(photo, *data.vphoto());
		}
	}, [&](const MTPDmessageMediaDocument &data) {
		const auto document = local->document();
		if (document && data.vdocument()) {
			owner.documentConvert(document, *data.vdocument());
		}
	}, [](const auto &) {
	});
}

void Chats::removePlaceholder(FullMsgId id) {
	const auto item = id ? _session->data().message(id) : nullptr;
	if (!item) {
		return;
	}
	if (const auto media = item->media()) {
		if (const auto photo = media->photo()) {
			photo->uploadingData = nullptr;
		} else if (const auto document = media->document()) {
			document->uploadingData = nullptr;
		}
	}
	item->destroy();
}

void Chats::clearQueue(not_null<Chat*> chat) {
	for (auto &service : chat->state.services) {
		ClearBytes(service.serialized);
	}
	chat->state.services.clear();
	for (auto &file : chat->state.queuedFiles) {
		ClearBytes(file.serialized);
		ClearBytes(file.record);
	}
	chat->state.queuedFiles.clear();
	for (const auto &queued : base::take(chat->queue)) {
		if (queued) {
			removePlaceholder(queued->placeholder);
		}
	}
}

void Chats::readInbox(
		not_null<SecretChatData*> peer,
		MsgId tillId,
		bool ignoreGhost) {
	const auto chat = lookup(peer->secretChatId());
	if (!chat || !tillId) {
		return;
	}
	auto maxDate = TimeId(0);
	auto stillUnread = 0;
	auto changed = false;
	for (auto &record : chat->records) {
		if (record.out) {
			continue;
		} else if (record.id > tillId) {
			stillUnread += record.unread ? 1 : 0;
			continue;
		}
		accumulate_max(maxDate, record.date);
		if (record.unread) {
			record.unread = false;
			changed = true;
		}
		if (record.ttl > 0 && !record.destroyAt && !record.mediaUnread) {
			record.destroyAt = DestroyAt(record.ttl);
			changed = true;
		}
	}
	if (changed) {
		saveHistory(chat);
		checkTtl();
	}
	session().data().history(peer)->inboxRead(tillId, stillUnread);

	const auto &ghost = AyuSettings::ghost(_session);
	if (maxDate <= chat->readDateSent
		|| chat->state.state != SecretChatData::State::Ready
		|| (!ignoreGhost && !ghost.sendReadMessages())) {
		return;
	}
	chat->readDateSent = maxDate;
	_api.request(base::take(chat->readRequestId)).cancel();
	const auto id = peer->secretChatId();
	const auto finish = [=] {
		if (const auto entry = lookup(id)) {
			entry->readRequestId = 0;
		}
	};
	chat->readRequestId = _api.request(MTPmessages_ReadEncryptedHistory(
		MTP_inputEncryptedChat(
			MTP_int(chat->state.chatId),
			MTP_long(chat->state.accessHash)),
		MTP_int(maxDate)
	)).done(finish).fail([=](const MTP::Error &error) {
		DEBUG_LOG(("Secret Error: chat %1 could not send a read, %2."
			).arg(SecretChatIdToServer(id)
			).arg(error.type()));
		finish();
	}).send();
}

void Chats::readContents(not_null<HistoryItem*> item) {
	const auto peer = item->history()->peer->asSecretChat();
	const auto chat = peer ? lookup(peer->secretChatId()) : nullptr;
	const auto record = chat ? FindRecord(chat->records, item->id) : nullptr;
	if (!record
		|| record->out
		|| !record->mediaUnread
		|| !RequiresContentRead(*record)) {
		return;
	}
	record->mediaUnread = false;
	if (!record->destroyAt) {
		record->destroyAt = DestroyAt(record->ttl);
	}
	saveHistory(chat);
	checkTtl();
	if (!AyuSettings::ghost(_session).sendReadMessages()) {
		return;
	}
	auto ids = QVector<SecretTLlong>{ secret_long(record->randomId) };
	sendService(chat, secret_decryptedMessageActionReadMessages(
		secret_vector<SecretTLlong>(std::move(ids))));
}

void Chats::sendTyping(not_null<SecretChatData*> peer) {
	const auto chat = lookup(peer->secretChatId());
	if (!chat || chat->state.state != SecretChatData::State::Ready) {
		return;
	}
	_api.request(base::take(chat->typingRequestId)).cancel();
	const auto id = peer->secretChatId();
	const auto finish = [=] {
		if (const auto entry = lookup(id)) {
			entry->typingRequestId = 0;
		}
	};
	chat->typingRequestId = _api.request(MTPmessages_SetEncryptedTyping(
		MTP_inputEncryptedChat(
			MTP_int(chat->state.chatId),
			MTP_long(chat->state.accessHash)),
		MTP_bool(true)
	)).done(finish).fail(finish).send();
}

void Chats::reportSpam(not_null<SecretChatData*> peer) {
	const auto chat = lookup(peer->secretChatId());
	if (!chat) {
		return;
	}
	_api.request(MTPmessages_ReportEncryptedSpam(
		MTP_inputEncryptedChat(
			MTP_int(chat->state.chatId),
			MTP_long(chat->state.accessHash))
	)).send();
}

void Chats::ackOutgoingUpTo(not_null<Chat*> chat, int32 hisInSeqNo) {
	auto &unacked = chat->state.unacked;
	auto changed = false;
	for (auto i = begin(unacked); i != end(unacked);) {
		if (i->outSeqNo < hisInSeqNo
			&& i->pendingRecord.isEmpty()) {
			ClearBytes(i->serialized);
			i = unacked.erase(i);
			changed = true;
		} else {
			++i;
		}
	}
	if (changed) {
		saveStates();
	}
}

SecretChatData *Chats::chat(SecretChatId id) const {
	const auto entry = lookup(id);
	return entry ? entry->peer.get() : nullptr;
}

ChatState *Chats::state(not_null<SecretChatData*> peer) {
	const auto entry = lookup(peer->secretChatId());
	return entry ? &entry->state : nullptr;
}

MessageIdsList Chats::search(
		not_null<SecretChatData*> peer,
		const QString &query) const {
	auto result = MessageIdsList();
	const auto chat = lookup(peer->secretChatId());
	const auto words = TextUtilities::PrepareSearchWords(query);
	if (!chat || words.isEmpty()) {
		return result;
	}
	const auto &owner = session().data();
	for (const auto &record : chat->records | ranges::views::reverse) {
		if (record.serviceKind != ServiceKind::None) {
			continue;
		}
		const auto parts = TextUtilities::PrepareSearchWords(
			record.text.text);
		const auto matches = ranges::all_of(words, [&](const QString &word) {
			return ranges::any_of(parts, [&](const QString &part) {
				return part.startsWith(word);
			});
		});
		if (matches && owner.message(peer->id, record.id)) {
			result.emplace_back(peer->id, record.id);
		}
	}
	return result;
}

std::vector<MsgId> Chats::sharedMedia(
		not_null<SecretChatData*> peer,
		Storage::SharedMediaType type) const {
	auto result = std::vector<MsgId>();
	const auto chat = lookup(peer->secretChatId());
	if (!chat) {
		return result;
	}
	const auto &owner = session().data();
	for (const auto &record : chat->records) {
		const auto item = owner.message(peer->id, record.id);
		if (item && item->sharedMediaTypes().test(type)) {
			result.push_back(record.id);
		}
	}
	return result;
}

rpl::producer<not_null<SecretChatData*>> Chats::chatAdded() const {
	return _chatAdded.events();
}

void Chats::saveStates() {
	if (!_saveTimer.isActive()) {
		_saveTimer.callOnce(kSaveDelay);
	}
}

void Chats::flush() {
	if (_saveTimer.isActive()) {
		_saveTimer.cancel();
		writeStates();
	}
	if (_historyTimer.isActive()) {
		_historyTimer.cancel();
		writeHistories();
	}
}

Chats::Chat *Chats::lookup(SecretChatId id) const {
	const auto i = _chats.find(id);
	return (i != end(_chats)) ? i->second.get() : nullptr;
}

not_null<Chats::Chat*> Chats::emplace(ChatState &&state) {
	const auto id = SecretChatIdFromServer(state.chatId);
	const auto peer = session().data().peer(peerFromSecretChat(id));
	const auto secret = peer->asSecretChat();
	Assert(secret != nullptr);

	if (state.pfs.stage == PfsState::Stage::Requested
		&& state.pfs.a.empty()) {
		state.pfs.stage = PfsState::Stage::None;
	}
	auto chat = std::make_unique<Chat>(Chat{
		.state = std::move(state),
		.peer = secret,
	});
	const auto i = _chats.emplace(id, std::move(chat)).first;
	const auto result = i->second.get();
	materialize(result);
	readHistory(result);
	requeueServices(result);
	for (const auto &file : result->state.queuedFiles) {
		auto message = ParseMessage(file.serialized);
		auto records = DeserializeHistory(file.record);
		if (!message || !records || records->size() != 1) {
			DEBUG_LOG(("Secret Error: chat %1 cannot restore queued file."
				).arg(result->state.chatId));
			continue;
		}
		result->queue.push_back(std::make_unique<QueuedMessage>(QueuedMessage{
			.message = std::move(*message),
			.randomId = file.randomId,
			.silent = file.silent,
			.file = InputFromOutgoingFile(file.file),
			.record = std::move(records->front()),
		}));
	}
	requeueUnsent(result);
	auto pendingFiles = std::vector<uint64>();
	auto changed = false;
	for (auto &message : result->state.unacked) {
		if (message.pendingRecord.isEmpty()) {
			continue;
		}
		const auto i = result->byRandomId.find(message.randomId);
		const auto record = (i != end(result->byRandomId))
			? FindRecord(result->records, i->second)
			: nullptr;
		if (record && record->file) {
			message.file = OutgoingFile{
				.kind = OutgoingFile::Kind::Existing,
				.id = record->file->id,
				.accessHash = record->file->accessHash,
			};
			ClearBytes(message.pendingRecord);
			changed = true;
		} else {
			pendingFiles.push_back(message.randomId);
		}
	}
	if (changed) {
		ackOutgoingUpTo(result, result->state.hisInSeqNo);
		saveStates();
	}
	for (const auto randomId : pendingFiles) {
		const auto i = ranges::find(
			result->state.unacked,
			randomId,
			&UnackedMessage::randomId);
		if (i != end(result->state.unacked)) {
			resendUnacked(result, *i);
		}
	}
	if (!result->state.held.empty()) {
		result->state.resendRequestedUpTo = 0;
		requestResend(result);
	}
	if (!result->state.creator
		&& result->state.state == SecretChatData::State::Requested
		&& !result->state.gA.empty()) {
		acceptRequested(result, result->state.gA);
	}
	_chatAdded.fire_copy(result->peer);
	return result;
}

not_null<Chats::Chat*> Chats::applyFields(
		SecretChatId id,
		int64 accessHash,
		UserId adminId,
		UserId participantId,
		TimeId date) {
	const auto self = session().userId();
	const auto creator = (adminId == self);
	const auto userId = (participantId == self) ? adminId : participantId;
	if (const auto existing = lookup(id)) {
		existing->state.accessHash = accessHash;
		existing->state.userId = userId;
		existing->state.creator = creator;
		existing->state.date = date;
		materialize(existing);
		return existing;
	}
	auto state = ChatState();
	state.chatId = SecretChatIdToServer(id);
	state.accessHash = accessHash;
	state.userId = userId;
	state.creator = creator;
	state.date = date;
	return emplace(std::move(state));
}

void Chats::materialize(not_null<Chat*> chat) {
	const auto peer = chat->peer;
	if (chat->state.userId) {
		peer->setUser(session().data().user(chat->state.userId));
	}
	peer->setCreator(chat->state.creator);
	peer->setTtl(chat->state.ttl);
	peer->setLayerHis(chat->state.layerHis);
	peer->setState(chat->state.state);
	peer->setLoadedStatus(PeerData::LoadedStatus::Full);

	// A secret history has no server dialog, so nothing ever fills the
	// fields a chats list row is computed from. Applying empty dialog
	// fields makes the folder, the counters and the last message known,
	// which is what stops requestChatListMessage() and readInbox() from
	// asking the server about a peer it does not know under this id.
	const auto history = session().data().history(peer);
	if (!history->folderKnown()) {
		history->applyDialogFields(nullptr, 0, MsgId(0), MsgId(0));
	}
	if (!history->lastMessageKnown()) {
		history->applyDialogTopMessage(MsgId(0));
	}
	if (chat->state.date && !history->chatListTimeId()) {
		history->setChatListTimeId(chat->state.date);
	}
	history->markLoadedAtTop();
	history->updateChatListExistence();
	session().data().notifySettings().request(peer);
}

void Chats::setChatState(not_null<Chat*> chat, SecretChatData::State state) {
	if (chat->state.state == state) {
		return;
	}
	chat->state.state = state;
	chat->peer->setState(state);
	DEBUG_LOG(("Secret Info: chat %1 is now %2."
		).arg(chat->state.chatId
		).arg(StateName(state)));
}

void Chats::acceptRequested(not_null<Chat*> chat, bytes::vector gA) {
	if (!chat->state.key.empty()) {
		DEBUG_LOG(("Secret Info: chat %1 already has a key, "
			"the request is ignored."
			).arg(chat->state.chatId));
		return;
	} else if (chat->accepting
		|| chat->state.state == SecretChatData::State::Closed) {
		return;
	}
	setChatState(chat, SecretChatData::State::Requested);
	chat->accepting = true;
	if (chat->state.gA != gA) {
		chat->state.gA = gA;
		saveStates();
	}

	const auto id = chat->peer->secretChatId();
	_dh.request([=](const Crypto::DhConfig *config) {
		const auto entry = lookup(id);
		if (!entry) {
			return;
		} else if (!config) {
			entry->accepting = false;
			DEBUG_LOG(("Secret Error: chat %1 has no group to answer in."
				).arg(entry->state.chatId));
			return;
		}
		acceptWithConfig(entry, gA, *config);
	});
}

void Chats::acceptWithConfig(
		not_null<Chat*> chat,
		bytes::const_span gA,
		const Crypto::DhConfig &config) {
	if (!chat->state.key.empty()
		|| chat->state.state == SecretChatData::State::Closed) {
		chat->accepting = false;
		return;
	}
	auto answer = MakeAcceptorAnswer(config, gA);
	if (answer.key.empty()) {
		chat->accepting = false;
		closeOnError(chat, u"the request could not be answered"_q);
		return;
	}
	const auto id = chat->peer->secretChatId();
	const auto fingerprint = answer.keyFingerprint;
	auto replaced = takePendingKey(id);
	Crypto::ZeroAndClear(replaced);
	_pendingKeys.emplace(id, std::move(answer.key));
	_api.request(MTPmessages_AcceptEncryption(
		MTP_inputEncryptedChat(
			MTP_int(chat->state.chatId),
			MTP_long(chat->state.accessHash)),
		MTP_bytes(answer.gB),
		MTP_long(fingerprint)
	)).done([=](const MTPEncryptedChat &result) {
		auto key = takePendingKey(id);
		const auto entry = lookup(id);
		if (!entry) {
			Crypto::ZeroAndClear(key);
			return;
		}
		entry->accepting = false;
		if (key.empty() || !entry->state.key.empty()) {
			Crypto::ZeroAndClear(key);
		} else {
			applyKey(entry, std::move(key), fingerprint);
		}
		handleEncryption(result);
	}).fail([=](const MTP::Error &error) {
		auto key = takePendingKey(id);
		if (const auto entry = lookup(id)) {
			entry->accepting = false;
			if (error.type() == u"ENCRYPTION_ALREADY_ACCEPTED"_q
				&& entry->state.key.empty()
				&& !key.empty()) {
				applyKey(entry, std::move(key), fingerprint);
				return;
			}
			DEBUG_LOG(("Secret Error: chat %1 was not accepted, %2."
				).arg(entry->state.chatId
				).arg(error.type()));
			if (!MTP::IsTemporaryError(error)) {
				closeOnError(entry, u"the request could not be accepted"_q);
			}
		}
		Crypto::ZeroAndClear(key);
	}).send();
}

void Chats::createWithConfig(
		not_null<UserData*> user,
		const Crypto::DhConfig &config,
		Fn<void(SecretChatData*)> done) {
	auto request = MakeCreatorRequest(config);
	if (request.gA.empty()) {
		Crypto::ZeroAndClear(request.a);
		DEBUG_LOG(("Secret Error: could not prepare a request for user %1."
			).arg(user->id.value));
		done(nullptr);
		return;
	}
	auto randomId = base::RandomValue<int32>();
	while (!randomId || _pendingSecrets.contains(randomId)) {
		randomId = base::RandomValue<int32>();
	}
	_pendingSecrets.emplace(randomId, std::move(request.a));
	_api.request(MTPmessages_RequestEncryption(
		user->inputUser(),
		MTP_int(randomId),
		MTP_bytes(request.gA)
	)).done([=](const MTPEncryptedChat &result) {
		auto secret = takePendingSecret(randomId);
		handleEncryption(result);
		const auto id = SecretChatIdFromServer(
			result.match([](const auto &data) { return data.vid(); }));
		const auto entry = lookup(id);
		if (!entry
			|| !entry->state.a.empty()
			|| !entry->state.key.empty()
			|| entry->state.state == SecretChatData::State::Closed) {
			Crypto::ZeroAndClear(secret);
		} else {
			entry->state.a = std::move(secret);
			saveStates();
			finishCreator(entry, true);
		}
		done(entry ? entry->peer.get() : nullptr);
	}).fail([=](const MTP::Error &error) {
		auto secret = takePendingSecret(randomId);
		Crypto::ZeroAndClear(secret);
		DEBUG_LOG(("Secret Error: requestEncryption failed, %1."
			).arg(error.type()));
		done(nullptr);
	}).send();
}

void Chats::finishCreator(not_null<Chat*> chat, bool allowConfigRequest) {
	if (chat->answerGPow.empty()
		|| !chat->state.key.empty()
		|| chat->state.state == SecretChatData::State::Closed) {
		return;
	} else if (chat->state.a.empty()) {
		// A restart between requestEncryption and its answer drops the
		// secret, because it is stored only once the chat has an id. The
		// chat is then stuck Waiting until it is discarded, which is 1.12.
		DEBUG_LOG(("Secret Info: chat %1 was answered, but the exchange "
			"it belongs to is gone."
			).arg(chat->state.chatId));
		return;
	}
	const auto config = _dh.cached();
	if (!config) {
		// A restart between the request and the answer leaves the chat
		// Waiting with a stored `a` and no group to raise it in, so the
		// answer waits for one more getDhConfig. The answer sits on the
		// chat rather than in the callback, which is what lets the second
		// pass find it again after that gap.
		if (!allowConfigRequest) {
			DEBUG_LOG(("Secret Error: chat %1 has no group to finish in."
				).arg(chat->state.chatId));
			return;
		}
		const auto id = chat->peer->secretChatId();
		_dh.request([=](const Crypto::DhConfig *fetched) {
			const auto entry = lookup(id);
			if (!entry) {
				return;
			} else if (!fetched) {
				DEBUG_LOG(("Secret Error: chat %1 has no group to "
					"finish in."
					).arg(entry->state.chatId));
				return;
			}
			finishCreator(entry, false);
		});
		return;
	}
	auto answer = MakeCreatorKey(*config, chat->answerGPow, chat->state.a);
	chat->answerGPow.clear();
	if (answer.key.empty()
		|| answer.keyFingerprint != chat->answerFingerprint) {
		Crypto::ZeroAndClear(answer.key);
		ClearKeyMaterial(chat->state);
		DEBUG_LOG(("Secret Error: chat %1 was answered with a key we "
			"cannot reproduce, closing it."
			).arg(chat->state.chatId));
		setChatState(chat, SecretChatData::State::Closed);
		saveStates();
		return;
	}
	Crypto::ZeroAndClear(chat->state.a);
	applyKey(chat, std::move(answer.key), answer.keyFingerprint);
}

void Chats::applyKey(
		not_null<Chat*> chat,
		bytes::vector &&key,
		int64 keyFingerprint) {
	if (chat->state.keyHash.empty()) {
		chat->state.keyHash = Crypto::KeyHashForVisualization(key);
	}
	Crypto::ZeroAndClear(chat->state.key);
	chat->state.key = std::move(key);
	chat->state.keyFingerprint = keyFingerprint;
	chat->state.keySetDate = base::unixtime::now();
	chat->state.messagesSinceKey = 0;
	chat->state.gA.clear();
	chat->answerGPow.clear();
	chat->answerFingerprint = 0;
	chat->accepting = false;
	setChatState(chat, SecretChatData::State::Ready);
	saveStates();
	sendNotifyLayer(chat);
}

void Chats::sendNotifyLayer(not_null<Chat*> chat) {
	enqueue(chat, MakeService(secret_decryptedMessageActionNotifyLayer(
		secret_int(kCurrentLayer))));
}

void Chats::enqueue(
		not_null<Chat*> chat,
		std::unique_ptr<QueuedMessage> message) {
	if (message->file && message->record) {
		const auto file = OutgoingFileFromInput(*message->file);
		const auto record = SerializeHistory(
			std::vector<HistoryRecord>{ *message->record });
		if (!file || !record) {
			removePlaceholder(message->placeholder);
			Ui::Toast::Show(tr::ayu_SecretChatFileFailed(tr::now));
			return;
		}
		chat->state.queuedFiles.push_back({
			.randomId = message->randomId,
			.serialized = SerializeMessage(message->message),
			.file = *file,
			.record = std::move(*record),
			.silent = message->silent,
		});
		saveStates();
		flush();
		Storage::details::Sync();
	}
	if (message->service) {
		chat->state.services.push_back({
			.randomId = message->randomId,
			.serialized = SerializeMessage(message->message),
		});
		saveStates();
	}
	chat->queue.push_back(std::move(message));
	sendNext(chat);
}

void Chats::sendNext(not_null<Chat*> chat) {
	if (chat->sending
		|| chat->queue.empty()
		|| !_statesWritable
		|| chat->state.key.empty()
		|| chat->state.state != SecretChatData::State::Ready) {
		return;
	}
	auto pendingRecord = chat->queue.front()->record
		? SerializeHistory(std::vector<HistoryRecord>{
			*chat->queue.front()->record,
		})
		: std::make_optional(QByteArray());
	if (!pendingRecord) {
		clearQueue(chat);
		saveStates();
		flush();
		Storage::details::Sync();
		return;
	}
	const auto queued = std::move(chat->queue.front());
	auto payload = PrepareOutgoing(chat->state, queued->message);
	if (payload.encrypted.empty()) {
		DEBUG_LOG(("Secret Error: chat %1 cannot encrypt, %2 queued "
			"messages are dropped."
			).arg(chat->state.chatId
			).arg(chat->queue.size()));
		clearQueue(chat);
		removePlaceholder(queued->placeholder);
		saveStates();
		flush();
		Storage::details::Sync();
		return;
	}
	chat->queue.erase(begin(chat->queue));
	chat->state.queuedFiles.erase(ranges::remove_if(
		chat->state.queuedFiles,
		[&](QueuedFile &file) {
			if (file.randomId != queued->randomId) {
				return false;
			}
			ClearBytes(file.serialized);
			ClearBytes(file.record);
			return true;
		}), end(chat->state.queuedFiles));
	chat->sending = true;
	chat->sendingRandomId = queued->randomId;
	chat->sendingPlaceholder = queued->placeholder;
	++chat->state.outCount;
	chat->state.unacked.push_back({
		.outSeqNo = payload.outSeqNo,
		.randomId = queued->randomId,
		.serialized = payload.serialized,
		.file = queued->file
			? OutgoingFileFromInput(*queued->file)
			: std::nullopt,
		.pendingRecord = std::move(*pendingRecord),
	});
	markSent(chat, queued->randomId);
	if (queued->service) {
		const auto randomId = queued->randomId;
		for (auto i = begin(chat->state.services);
				i != end(chat->state.services);) {
			if (i->randomId != randomId) {
				++i;
				continue;
			}
			ClearBytes(i->serialized);
			i = chat->state.services.erase(i);
		}
	}
	if (chat->commitRandomId && chat->commitRandomId == queued->randomId) {
		switchToNewKey(chat);
	}
	countMessage(chat);
	saveStates();
	flush();
	Storage::details::Sync();

	const auto id = chat->peer->secretChatId();
	const auto randomId = queued->randomId;
	const auto outSeqNo = payload.outSeqNo;
	const auto input = MTP_inputEncryptedChat(
		MTP_int(chat->state.chatId),
		MTP_long(chat->state.accessHash));
	const auto data = MTP_bytes(payload.encrypted);
	const auto placeholder = queued->placeholder;
	const auto pendingFile = queued->file.has_value();
	const auto resendRequest = queued->message.match([](
			const SecretTLDdecryptedMessageService &data) {
		return data.vaction().type() == secretc_decryptedMessageActionResend;
	}, [](const auto &) {
		return false;
	});
	const auto done = [=](const MTPmessages_SentEncryptedMessage &result) {
		const auto entry = lookup(id);
		if (!entry) {
			removePlaceholder(placeholder);
			return;
		} else if (entry->sendingRandomId != randomId) {
			return;
		}
		entry->sending = false;
		entry->sendingRandomId = 0;
		entry->sendingPlaceholder = FullMsgId();
		entry->sendRequestId = 0;
		const auto date = result.match([](const auto &data) {
			return data.vdate().v;
		});
		if (pendingFile) {
			if (!finishPendingFile(entry, randomId, result, placeholder)) {
				replaceUnacked(entry, randomId);
				removePlaceholder(placeholder);
				Ui::Toast::Show(tr::ayu_SecretChatFileFailed(tr::now));
			}
		} else {
			setRecordDate(entry, randomId, date);
		}
		sendNext(entry);
	};
	const auto fail = [=](const MTP::Error &error) {
		const auto entry = lookup(id);
		if (!entry) {
			return;
		} else if (entry->sendingRandomId != randomId) {
			return;
		}
		entry->sending = false;
		entry->sendingRandomId = 0;
		entry->sendingPlaceholder = FullMsgId();
		entry->sendRequestId = 0;
		if (resendRequest && !entry->state.held.empty()) {
			entry->state.resendRequestedUpTo = 0;
			saveStates();
			_resendRetryTimer.callOnce(kResendRetryDelay);
		}
		if (pendingFile) {
			replaceUnacked(entry, randomId);
			removePlaceholder(placeholder);
			Ui::Toast::Show(tr::ayu_SecretChatFileFailed(tr::now));
		}
		DEBUG_LOG(("Secret Error: chat %1 could not send %2, %3."
			).arg(entry->state.chatId
			).arg(outSeqNo
			).arg(error.type()));
		sendNext(entry);
	};
	if (queued->service) {
		chat->sendRequestId = _api.request(MTPmessages_SendEncryptedService(
			input,
			MTP_long(randomId),
			data
		)).done(done).fail(fail).send();
	} else if (queued->file) {
		using Flag = MTPmessages_SendEncryptedFile::Flag;
		chat->sendRequestId = _api.request(MTPmessages_SendEncryptedFile(
			MTP_flags(queued->silent ? Flag::f_silent : Flag()),
			input,
			MTP_long(randomId),
			data,
			*queued->file
		)).done(done).fail(fail).send();
	} else {
		using Flag = MTPmessages_SendEncrypted::Flag;
		chat->sendRequestId = _api.request(MTPmessages_SendEncrypted(
			MTP_flags(queued->silent ? Flag::f_silent : Flag()),
			input,
			MTP_long(randomId),
			data
		)).done(done).fail(fail).send();
	}
}

bool Chats::finishPendingFile(
		not_null<Chat*> chat,
		uint64 randomId,
		const MTPmessages_SentEncryptedMessage &result,
		FullMsgId placeholder) {
	auto pending = static_cast<UnackedMessage*>(nullptr);
	for (auto &message : chat->state.unacked) {
		if (message.randomId == randomId) {
			pending = &message;
			break;
		}
	}
	if (!pending || pending->pendingRecord.isEmpty()) {
		return false;
	}
	const auto decoded = DeserializeHistory(pending->pendingRecord);
	if (!decoded || decoded->size() != 1) {
		return false;
	}
	auto sent = decoded->front();
	const auto file = result.match([](
			const MTPDmessages_sentEncryptedFile &data)
			-> std::optional<EncryptedFileInfo> {
		return data.vfile().match([](const MTPDencryptedFile &file)
				-> std::optional<EncryptedFileInfo> {
			return FileInfo(file);
		}, [](const MTPDencryptedFileEmpty &)
				-> std::optional<EncryptedFileInfo> {
			return std::nullopt;
		});
	}, [](const MTPDmessages_sentEncryptedMessage &)
			-> std::optional<EncryptedFileInfo> {
		return std::nullopt;
	});
	if (!file) {
		return false;
	}
	if (!chat->byRandomId.contains(randomId)) {
		sent.id = chat->state.nextMsgId++;
		sent.file = *file;
		const auto date = result.match([](const auto &data) {
			return data.vdate().v;
		});
		sent.date = date ? date : sent.date;
		adoptPlaceholderMedia(placeholder, sent);
		removePlaceholder(placeholder);
		appendRecord(chat, std::move(sent));
		flush();
		Storage::details::Sync();
	} else {
		removePlaceholder(placeholder);
	}
	pending->file = OutgoingFile{
		.kind = OutgoingFile::Kind::Existing,
		.id = file->id,
		.accessHash = file->accessHash,
	};
	if (chat->historyWritable) {
		ClearBytes(pending->pendingRecord);
		ackOutgoingUpTo(chat, chat->state.hisInSeqNo);
	}
	saveStates();
	flush();
	Storage::details::Sync();
	chat->pendingFileRetries.erase(ranges::remove(
		chat->pendingFileRetries,
		randomId), end(chat->pendingFileRetries));
	return true;
}

void Chats::appendRecord(not_null<Chat*> chat, HistoryRecord &&record) {
	const auto randomId = record.randomId;
	const auto id = record.id;
	registerFile(record);
	chat->records.push_back(std::move(record));
	if (randomId) {
		chat->byRandomId.emplace(randomId, id);
	}
	AddHistoryItem(chat->peer, chat->records.back());
	saveHistory(chat);
	saveStates();
}

void Chats::registerFile(const HistoryRecord &record) {
	const auto media = record.file ? ParseMedia(record.media) : std::nullopt;
	auto file = media ? FileFromMedia(*media) : std::nullopt;
	if (file) {
		_files[record.file->id] = std::move(*file);
	}
}

void Chats::unregisterFile(const HistoryRecord &record) {
	if (!record.file) {
		return;
	}
	if (const auto media = ParseMedia(record.media)) {
		switch (media->type()) {
		case secretc_decryptedMessageMediaPhoto8:
		case secretc_decryptedMessageMediaPhoto: {
			session().data().photo(PhotoId(record.file->id))->clearLocalCache();
		} break;
		default: {
			const auto document = session().data().document(
				DocumentId(record.file->id));
			document->cancel();
			if (const auto active = document->activeMediaView()) {
				active->setBytes(QByteArray());
			}
			session().data().cache().remove(document->cacheKey());
			session().data().cache().remove(
				document->goodThumbnailCacheKey());
		} break;
		}
	}
	const auto i = _files.find(record.file->id);
	if (i != end(_files)) {
		Crypto::ZeroAndClear(i->second.key);
		_files.erase(i);
	}
}

const MediaFile *Chats::file(uint64 id) const {
	const auto i = _files.find(id);
	return (i != end(_files)) ? &i->second : nullptr;
}

void Chats::setRecordDate(
		not_null<Chat*> chat,
		uint64 randomId,
		TimeId date) {
	const auto i = chat->byRandomId.find(randomId);
	if (!date || i == end(chat->byRandomId)) {
		return;
	}
	const auto record = FindRecord(chat->records, i->second);
	if (!record || record->date == date) {
		return;
	}
	// The visible item keeps the date it was created with, because a
	// message that is already in the history has no public way to change
	// its date. The record is what the read-up-to-date lookups go through,
	// so that is where the server date belongs.
	record->date = date;
	saveHistory(chat);
}

void Chats::markSent(not_null<Chat*> chat, uint64 randomId) {
	const auto i = chat->byRandomId.find(randomId);
	if (i == end(chat->byRandomId)) {
		return;
	}
	const auto record = FindRecord(chat->records, i->second);
	if (record && record->unsent) {
		record->unsent = false;
		saveHistory(chat);
	}
}

void Chats::requeueUnsent(not_null<Chat*> chat) {
	const auto encrypted = [&](uint64 randomId) {
		return ranges::contains(
			chat->state.unacked,
			randomId,
			&UnackedMessage::randomId);
	};
	for (auto &record : chat->records) {
		if (!record.unsent) {
			continue;
		} else if (encrypted(record.randomId)) {
			record.unsent = false;
			saveHistory(chat);
			continue;
		}
		const auto replied = record.replyToLocalId
			? FindRecord(chat->records, record.replyToLocalId)
			: nullptr;
		auto media = record.media.isEmpty()
			? std::nullopt
			: ParseMedia(record.media);
		DEBUG_LOG(("Secret Info: chat %1 queues unsent message %2 again."
			).arg(chat->state.chatId
			).arg(record.id.bare));
		chat->queue.push_back(std::make_unique<QueuedMessage>(QueuedMessage{
			.message = MakeTextMessage(
				_session,
				record.randomId,
				record.ttl,
				record.text,
				replied ? replied->randomId : uint64(0),
				record.silent,
				std::move(media),
				record.groupedId),
			.randomId = record.randomId,
			.silent = record.silent,
		}));
	}
	sendNext(chat);
}

void Chats::requeueServices(not_null<Chat*> chat) {
	const auto &pfs = chat->state.pfs;
	auto kept = std::vector<PendingService>();
	for (auto &service : base::take(chat->state.services)) {
		auto message = ParseMessage(service.serialized);
		if (!message || !IsService(*message)) {
			ClearBytes(service.serialized);
			continue;
		}
		message->match([&](const SecretTLDdecryptedMessageService &data) {
			data.vaction().match([&](
					const SecretTLDdecryptedMessageActionCommitKey &commit) {
				if (pfs.stage == PfsState::Stage::Accepted
					&& int64(commit.vexchange_id().v) == pfs.exchangeId) {
					chat->commitRandomId = service.randomId;
				}
			}, [](const auto &) {
			});
		}, [](const auto &) {
		});
		DEBUG_LOG(("Secret Info: chat %1 queues an unsent action again."
			).arg(chat->state.chatId));
		chat->queue.push_back(std::make_unique<QueuedMessage>(QueuedMessage{
			.message = std::move(*message),
			.randomId = service.randomId,
			.service = true,
		}));
		kept.push_back(std::move(service));
	}
	chat->state.services = std::move(kept);
}

bytes::vector Chats::takePendingSecret(int32 randomId) {
	const auto i = _pendingSecrets.find(randomId);
	if (i == end(_pendingSecrets)) {
		return {};
	}
	auto result = std::move(i->second);
	_pendingSecrets.erase(i);
	return result;
}

bytes::vector Chats::takePendingKey(SecretChatId id) {
	const auto i = _pendingKeys.find(id);
	if (i == end(_pendingKeys)) {
		return {};
	}
	auto result = std::move(i->second);
	_pendingKeys.erase(i);
	return result;
}

void Chats::handleEncryption(const MTPEncryptedChat &chat) {
	chat.match([&](const MTPDencryptedChatRequested &data) {
		const auto entry = applyFields(
			SecretChatIdFromServer(data.vid()),
			data.vaccess_hash().v,
			UserId(data.vadmin_id()),
			UserId(data.vparticipant_id()),
			data.vdate().v);
		acceptRequested(entry, bytes::make_vector(data.vg_a().v));
	}, [&](const MTPDencryptedChatWaiting &data) {
		const auto entry = applyFields(
			SecretChatIdFromServer(data.vid()),
			data.vaccess_hash().v,
			UserId(data.vadmin_id()),
			UserId(data.vparticipant_id()),
			data.vdate().v);
		setChatState(entry, SecretChatData::State::Waiting);
	}, [&](const MTPDencryptedChat &data) {
		const auto entry = applyFields(
			SecretChatIdFromServer(data.vid()),
			data.vaccess_hash().v,
			UserId(data.vadmin_id()),
			UserId(data.vparticipant_id()),
			data.vdate().v);
		const auto fingerprint = data.vkey_fingerprint().v;
		if (!entry->state.key.empty()) {
			if (entry->state.keyFingerprint != fingerprint) {
				ClearKeyMaterial(entry->state);
				DEBUG_LOG(("Secret Error: chat %1 was answered with "
					"another fingerprint, closing it."
					).arg(entry->state.chatId));
				setChatState(entry, SecretChatData::State::Closed);
			} else {
				setChatState(entry, SecretChatData::State::Ready);
			}
			return;
		} else if (!entry->state.creator) {
			DEBUG_LOG(("Secret Info: chat %1 was answered while we are "
				"still accepting it."
				).arg(entry->state.chatId));
			return;
		}
		entry->answerGPow = bytes::make_vector(data.vg_a_or_b().v);
		entry->answerFingerprint = fingerprint;
		finishCreator(entry, true);
	}, [&](const MTPDencryptedChatDiscarded &data) {
		const auto id = SecretChatIdFromServer(data.vid());
		const auto entry = lookup(id);
		if (!entry) {
			DEBUG_LOG(("Secret Info: discard for unknown chat %1."
				).arg(SecretChatIdToServer(id)));
			return;
		}
		clearQueue(entry);
		if (data.is_history_deleted()) {
			const auto peer = entry->peer;
			forgetChat(entry);
			session().data().deleteConversationLocally(peer);
			return;
		}
		ClearKeyMaterial(entry->state);
		entry->state.held.clear();
		setChatState(entry, SecretChatData::State::Closed);
	}, [&](const MTPDencryptedChatEmpty &data) {
		DEBUG_LOG(("Secret Info: empty chat %1.").arg(data.vid().v));
	});
	saveStates();
}

void Chats::handleNewMessage(const MTPEncryptedMessage &message) {
	struct Fields {
		int32 chatId = 0;
		TimeId date = 0;
		QByteArray bytes;
		std::optional<EncryptedFileInfo> file;
	};
	const auto fields = message.match([](const MTPDencryptedMessage &data) {
		auto file = std::optional<EncryptedFileInfo>();
		data.vfile().match([&](const MTPDencryptedFile &data) {
			file = FileInfo(data);
		}, [](const MTPDencryptedFileEmpty &) {
		});
		return Fields{
			.chatId = data.vchat_id().v,
			.date = data.vdate().v,
			.bytes = data.vbytes().v,
			.file = file,
		};
	}, [](const MTPDencryptedMessageService &data) {
		return Fields{
			.chatId = data.vchat_id().v,
			.date = data.vdate().v,
			.bytes = data.vbytes().v,
		};
	});
	_ackNeeded = true;

	const auto chat = lookup(SecretChatIdFromServer(fields.chatId));
	if (!chat) {
		DEBUG_LOG(("Secret Info: a message for unknown chat %1 is dropped."
			).arg(fields.chatId));
		return;
	} else if (chat->state.state != SecretChatData::State::Ready
		|| chat->state.key.empty()) {
		DEBUG_LOG(("Secret Info: chat %1 has no key yet, a message of %2 "
			"bytes is dropped."
			).arg(fields.chatId
			).arg(fields.bytes.size()));
		return;
	}
	const auto payload = bytes::make_span(fields.bytes);
	const auto fingerprint = Crypto::PayloadKeyFingerprint(payload);
	auto &pfs = chat->state.pfs;
	const auto current = (fingerprint == chat->state.keyFingerprint);
	if (!current
		&& (pfs.otherKey.empty() || fingerprint != pfs.otherFingerprint)) {
		DEBUG_LOG(("Secret Info: chat %1 got a message under another key, "
			"it is dropped."
			).arg(fields.chatId));
		return;
	}
	auto decrypted = Crypto::DecryptPayload(
		bytes::make_span(current ? chat->state.key : pfs.otherKey),
		fingerprint,
		!chat->state.creator,
		payload);
	if (!decrypted) {
		closeOnError(chat, u"a message failed to decrypt"_q);
		return;
	} else if (current
		&& pfs.stage == PfsState::Stage::None
		&& !pfs.otherKey.empty()) {
		Crypto::ZeroAndClear(pfs.otherKey);
		pfs.otherFingerprint = 0;
		saveStates();
	}
	const auto clearDecrypted = gsl::finally([&] {
		Crypto::ZeroAndClear(*decrypted);
	});
	if (decrypted->size() > size_t(std::numeric_limits<qsizetype>::max())) {
		closeOnError(chat, u"a message is too large"_q);
		return;
	}
	auto layer = ParseLayer(*decrypted);
	if (!layer) {
		closeOnError(chat, u"a message could not be read"_q);
		return;
	}
	layer->file = fields.file;
	receive(
		chat,
		std::move(*layer),
		QByteArray(
			reinterpret_cast<const char*>(decrypted->data()),
			qsizetype(decrypted->size())),
		fields.date);
}

void Chats::closeOnError(not_null<Chat*> chat, const QString &reason) {
	DEBUG_LOG(("Secret Error: chat %1 is closed, %2."
		).arg(chat->state.chatId
		).arg(reason));
	if (chat->state.state != SecretChatData::State::Closed) {
		sendDiscard(chat, false);
	}
	ClearKeyMaterial(chat->state);
	chat->state.held.clear();
	clearQueue(chat);
	setChatState(chat, SecretChatData::State::Closed);
	saveStates();
}

void Chats::sendDiscard(not_null<Chat*> chat, bool deleteHistory) {
	using Flag = MTPmessages_DiscardEncryption::Flag;
	const auto chatId = chat->state.chatId;
	_api.request(MTPmessages_DiscardEncryption(
		MTP_flags(deleteHistory ? Flag::f_delete_history : Flag(0)),
		MTP_int(chatId)
	)).fail([=](const MTP::Error &error) {
		DEBUG_LOG(("Secret Error: chat %1 could not be discarded, %2."
			).arg(chatId
			).arg(error.type()));
	}).send();
}

void Chats::raiseLayer(not_null<Chat*> chat, int layer) {
	if (layer <= chat->state.layerHis) {
		return;
	}
	chat->state.layerHis = layer;
	chat->peer->setLayerHis(layer);
	saveStates();
}

void Chats::receive(
		not_null<Chat*> chat,
		IncomingLayer &&layer,
		QByteArray &&serialized,
		TimeId date) {
	const auto clear = gsl::finally([&] { ClearBytes(serialized); });
	raiseLayer(chat, layer.layer);
	switch (CheckSeqNo(chat->state, layer.inSeqNo, layer.outSeqNo)) {
	case SeqCheck::Invalid: {
		closeOnError(chat, u"a message broke the sequence"_q);
	} break;
	case SeqCheck::Duplicate: {
		DEBUG_LOG(("Secret Info: chat %1 dropped repeated message %2."
			).arg(chat->state.chatId
			).arg(layer.outSeqNo));
	} break;
	case SeqCheck::Hole: {
		auto &held = chat->state.held;
		if (held.size() >= kMaxHeldMessages
			|| int64(SeqIndex(layer.outSeqNo))
				> int64(chat->state.inCount) + kMaxHeldMessages) {
			closeOnError(chat, u"the message sequence gap is too large"_q);
			return;
		}
		const auto i = ranges::lower_bound(
			held,
			layer.outSeqNo,
			ranges::less(),
			&HeldMessage::outSeqNo);
		if (i != end(held) && i->outSeqNo == layer.outSeqNo) {
			break;
		}
		held.insert(i, HeldMessage{
			.outSeqNo = layer.outSeqNo,
			.decrypted = std::move(serialized),
			.date = date,
			.file = layer.file,
		});
		DEBUG_LOG(("Secret Info: chat %1 holds message %2, expecting %3."
			).arg(chat->state.chatId
			).arg(layer.outSeqNo
			).arg(PeerOutSeqNo(chat->state, chat->state.inCount)));
		const auto answer = [&](const SecretTLDecryptedMessageAction &act) {
			act.match([&](const SecretTLDdecryptedMessageActionResend &data) {
				answerResend(
					chat,
					data.vstart_seq_no().v,
					data.vend_seq_no().v);
			}, [](const auto &) {
			});
		};
		layer.message.match([&](const SecretTLDdecryptedMessageService &d) {
			answer(d.vaction());
		}, [&](const SecretTLDdecryptedMessageService8 &d) {
			answer(d.vaction());
		}, [](const auto &) {
		});
		requestResend(chat);
		saveStates();
	} break;
	case SeqCheck::Accept: {
		accept(chat, layer, date, false);
		drainHeld(chat);
	} break;
	}
}

void Chats::accept(
		not_null<Chat*> chat,
		const IncomingLayer &layer,
		TimeId date,
		bool replay) {
	++chat->state.inCount;
	chat->state.hisInSeqNo = layer.inSeqNo;
	ackOutgoingUpTo(chat, layer.inSeqNo);
	applyMessage(chat, layer, date, replay);
	countMessage(chat);
	saveStates();
}

void Chats::drainHeld(not_null<Chat*> chat) {
	auto &held = chat->state.held;
	while (!held.empty()
		&& chat->state.state == SecretChatData::State::Ready
		&& SeqIndex(held.front().outSeqNo) <= chat->state.inCount) {
		auto message = std::move(held.front());
		held.erase(begin(held));
		auto layer = ParseLayer(bytes::make_span(message.decrypted));
		if (!layer) {
			closeOnError(chat, u"a held message could not be read"_q);
			return;
		}
		layer->file = message.file;
		switch (CheckSeqNo(chat->state, layer->inSeqNo, layer->outSeqNo)) {
		case SeqCheck::Accept: {
			accept(chat, *layer, message.date, true);
		} break;
		case SeqCheck::Duplicate: {
		} break;
		case SeqCheck::Hole:
		case SeqCheck::Invalid: {
			closeOnError(chat, u"a held message broke the sequence"_q);
		} return;
		}
	}
	requestResend(chat);
	saveStates();
}

void Chats::requestResend(not_null<Chat*> chat) {
	const auto &held = chat->state.held;
	if (held.empty()) {
		return;
	}
	const auto start = chat->state.inCount;
	const auto finish = SeqIndex(held.front().outSeqNo) - 1;
	if (finish < start || chat->state.resendRequestedUpTo > finish) {
		return;
	}
	chat->state.resendRequestedUpTo = finish + 1;
	DEBUG_LOG(("Secret Info: chat %1 asks to resend %2 to %3."
		).arg(chat->state.chatId
		).arg(PeerOutSeqNo(chat->state, start)
		).arg(PeerOutSeqNo(chat->state, finish)));
	enqueue(chat, MakeService(secret_decryptedMessageActionResend(
		secret_int(PeerOutSeqNo(chat->state, start)),
		secret_int(PeerOutSeqNo(chat->state, finish)))));
}

void Chats::answerResend(
		not_null<Chat*> chat,
		int32 startSeqNo,
		int32 endSeqNo) {
	const auto start = SeqIndex(startSeqNo);
	const auto finish = SeqIndex(endSeqNo);
	if (start < 0 || finish < start || finish - start > kMaxResendCount) {
		DEBUG_LOG(("Secret Error: chat %1 ignores a resend of %2 to %3."
			).arg(chat->state.chatId
			).arg(startSeqNo
			).arg(endSeqNo));
		return;
	}
	auto found = 0;
	for (const auto &message : chat->state.unacked) {
		const auto index = SeqIndex(message.outSeqNo);
		if (index >= start && index <= finish) {
			resendUnacked(chat, message);
			++found;
		}
	}
	DEBUG_LOG(("Secret Info: chat %1 resends %2 of %3 asked messages."
		).arg(chat->state.chatId
		).arg(found
		).arg(finish - start + 1));
}

void Chats::resendUnacked(
		not_null<Chat*> chat,
		const UnackedMessage &message) {
	const auto serialized = bytes::make_span(message.serialized);
	const auto layer = ParseLayer(serialized);
	const auto encrypted = Crypto::EncryptPayload(
		bytes::make_span(chat->state.key),
		chat->state.keyFingerprint,
		chat->state.creator,
		serialized);
	const auto chatId = chat->state.chatId;
	const auto outSeqNo = message.outSeqNo;
	const auto randomId = message.randomId;
	const auto pendingFile = !message.pendingRecord.isEmpty();
	if (!layer || encrypted.empty()) {
		DEBUG_LOG(("Secret Error: chat %1 cannot resend %2."
			).arg(chatId
			).arg(outSeqNo));
		return;
	}
	const auto input = MTP_inputEncryptedChat(
		MTP_int(chat->state.chatId),
		MTP_long(chat->state.accessHash));
	const auto fail = [=](const MTP::Error &error) {
		DEBUG_LOG(("Secret Error: chat %1 could not resend %2, %3."
			).arg(chatId
			).arg(outSeqNo
			).arg(error.type()));
		if (pendingFile) {
			if (const auto entry = lookup(SecretChatIdFromServer(chatId))) {
				if (!ranges::contains(entry->pendingFileRetries, randomId)) {
					entry->pendingFileRetries.push_back(randomId);
				}
				_resendRetryTimer.callOnce(kResendRetryDelay);
			}
		}
	};
	const auto i = chat->byRandomId.find(message.randomId);
	const auto record = (i != end(chat->byRandomId))
		? FindRecord(chat->records, i->second)
		: nullptr;
	const auto silent = layer->message.match([](
			const SecretTLDdecryptedMessage &data) {
		return data.is_silent();
	}, [](const auto &) {
		return false;
	});
	const auto requiresFile = layer->message.match([](
			const SecretTLDdecryptedMessage &data) {
		return data.vmedia() && FileFromMedia(*data.vmedia()).has_value();
	}, [](const SecretTLDdecryptedMessage46 &data) {
		return data.vmedia() && FileFromMedia(*data.vmedia()).has_value();
	}, [](const SecretTLDdecryptedMessage23 &data) {
		return FileFromMedia(data.vmedia()).has_value();
	}, [](const SecretTLDdecryptedMessage8 &data) {
		return FileFromMedia(data.vmedia()).has_value();
	}, [](const auto &) {
		return false;
	});
	if (requiresFile && !message.file && !(record && record->file)) {
		DEBUG_LOG(("Secret Error: chat %1 cannot resend file %2."
			).arg(chatId
			).arg(outSeqNo));
		return;
	}
	if (IsService(layer->message)) {
		_api.request(MTPmessages_SendEncryptedService(
			input,
			MTP_long(message.randomId),
			MTP_bytes(encrypted)
		)).fail(fail).send();
	} else if (message.file) {
		using Flag = MTPmessages_SendEncryptedFile::Flag;
		_api.request(MTPmessages_SendEncryptedFile(
			MTP_flags(silent ? Flag::f_silent : Flag()),
			input,
			MTP_long(message.randomId),
			MTP_bytes(encrypted),
			InputFromOutgoingFile(*message.file)
		)).done([=](const MTPmessages_SentEncryptedMessage &result) {
			if (const auto entry = lookup(SecretChatIdFromServer(chatId))) {
				finishPendingFile(entry, randomId, result, FullMsgId());
			}
		}).fail(fail).send();
	} else if (record && record->file) {
		using Flag = MTPmessages_SendEncryptedFile::Flag;
		_api.request(MTPmessages_SendEncryptedFile(
			MTP_flags(silent ? Flag::f_silent : Flag()),
			input,
			MTP_long(message.randomId),
			MTP_bytes(encrypted),
			MTP_inputEncryptedFile(
				MTP_long(record->file->id),
				MTP_long(record->file->accessHash))
		)).fail(fail).send();
	} else {
		using Flag = MTPmessages_SendEncrypted::Flag;
		_api.request(MTPmessages_SendEncrypted(
			MTP_flags(silent ? Flag::f_silent : Flag()),
			input,
			MTP_long(message.randomId),
			MTP_bytes(encrypted)
		)).fail(fail).send();
	}
}

void Chats::applyMessage(
		not_null<Chat*> chat,
		const IncomingLayer &layer,
		TimeId date,
		bool replay) {
	const auto &file = layer.file;
	layer.message.match([&](const SecretTLDdecryptedMessage &data) {
		receiveText(
			chat,
			data.vrandom_id().v,
			date,
			data.vttl().v,
			TextFromSecret(_session, data),
			data.vreply_to_random_id().value_or_empty(),
			data.vmedia(),
			file,
			data.vgrouped_id().value_or_empty());
	}, [&](const SecretTLDdecryptedMessage46 &data) {
		receiveText(
			chat,
			data.vrandom_id().v,
			date,
			data.vttl().v,
			TextFromSecret(_session, data),
			data.vreply_to_random_id().value_or_empty(),
			data.vmedia(),
			file);
	}, [&](const SecretTLDdecryptedMessage23 &data) {
		receiveText(
			chat,
			data.vrandom_id().v,
			date,
			data.vttl().v,
			TextWithEntities{ qs(data.vmessage()) },
			0,
			&data.vmedia(),
			file);
	}, [&](const SecretTLDdecryptedMessage8 &data) {
		receiveText(
			chat,
			data.vrandom_id().v,
			date,
			0,
			TextWithEntities{ qs(data.vmessage()) },
			0,
			&data.vmedia(),
			file);
	}, [&](const SecretTLDdecryptedMessageService &data) {
		applyAction(chat, data.vrandom_id().v, date, data.vaction(), replay);
	}, [&](const SecretTLDdecryptedMessageService8 &data) {
		applyAction(chat, data.vrandom_id().v, date, data.vaction(), replay);
	});
}

void Chats::applyAction(
		not_null<Chat*> chat,
		uint64 randomId,
		TimeId date,
		const SecretTLDecryptedMessageAction &action,
		bool replay) {
	action.match([&](const SecretTLDdecryptedMessageActionResend &data) {
		if (!replay) {
			answerResend(chat, data.vstart_seq_no().v, data.vend_seq_no().v);
		}
	}, [&](const SecretTLDdecryptedMessageActionNotifyLayer &data) {
		raiseLayer(chat, data.vlayer().v);
	}, [&](const SecretTLDdecryptedMessageActionSetMessageTTL &data) {
		const auto ttl = ClampTtl(data.vttl_seconds().v);
		chat->state.ttl = ttl;
		chat->peer->setTtl(ttl);
		saveStates();
		addService(
			chat,
			randomId,
			date,
			ServiceKind::SetTtl,
			QByteArray::number(ttl));
	}, [&](const SecretTLDdecryptedMessageActionDeleteMessages &data) {
		auto ids = std::vector<MsgId>();
		for (const auto &deleted : data.vrandom_ids().v) {
			const auto i = chat->byRandomId.find(deleted.v);
			if (i != end(chat->byRandomId)) {
				ids.push_back(i->second);
			}
		}
		removeRecords(chat, ids);
	}, [&](const SecretTLDdecryptedMessageActionReadMessages &data) {
		auto changed = false;
		for (const auto &read : data.vrandom_ids().v) {
			const auto i = chat->byRandomId.find(read.v);
			const auto record = (i != end(chat->byRandomId))
				? FindRecord(chat->records, i->second)
				: nullptr;
			if (record
				&& record->out
				&& record->mediaUnread
				&& RequiresContentRead(*record)) {
				record->mediaUnread = false;
				record->destroyAt = DestroyAt(record->ttl);
				changed = true;
			}
		}
		if (changed) {
			saveHistory(chat);
			checkTtl();
		}
	}, [&](const SecretTLDdecryptedMessageActionFlushHistory &) {
		removeRecords(chat, chat->records
			| ranges::views::transform(&HistoryRecord::id)
			| ranges::to_vector);
	}, [&](const SecretTLDdecryptedMessageActionScreenshotMessages &) {
		addService(chat, randomId, date, ServiceKind::Screenshot);
	}, [&](const SecretTLDdecryptedMessageActionRequestKey &data) {
		answerKeyRequest(
			chat,
			int64(data.vexchange_id().v),
			bytes::make_vector(data.vg_a().v));
	}, [&](const SecretTLDdecryptedMessageActionAcceptKey &data) {
		acceptKey(
			chat,
			int64(data.vexchange_id().v),
			bytes::make_span(data.vg_b().v),
			int64(data.vkey_fingerprint().v));
	}, [&](const SecretTLDdecryptedMessageActionAbortKey &data) {
		abortKey(chat, int64(data.vexchange_id().v));
	}, [&](const SecretTLDdecryptedMessageActionCommitKey &data) {
		commitKey(
			chat,
			int64(data.vexchange_id().v),
			int64(data.vkey_fingerprint().v));
	}, [&](const auto &) {
		DEBUG_LOG(("Secret Info: chat %1 skipped service action %2."
			).arg(chat->state.chatId
			).arg(action.type()));
	});
}

void Chats::addService(
		not_null<Chat*> chat,
		uint64 randomId,
		TimeId date,
		ServiceKind kind,
		QByteArray &&payload) {
	if (!randomId || chat->byRandomId.contains(randomId)) {
		return;
	}
	appendRecord(chat, HistoryRecord{
		.id = chat->state.nextMsgId++,
		.randomId = randomId,
		.unread = true,
		.date = date,
		.serviceKind = kind,
		.servicePayload = std::move(payload),
	});
}

void Chats::removeRecords(
		not_null<Chat*> chat,
		const std::vector<MsgId> &ids) {
	if (ids.empty()) {
		return;
	}
	dropRecords(chat, ids);
	RemoveHistoryItems(chat->peer, ids);
	updateUnreadCount(chat);
}

void Chats::dropRecords(
		not_null<Chat*> chat,
		const std::vector<MsgId> &ids) {
	auto &records = chat->records;
	auto randomIds = base::flat_set<uint64>();
	for (const auto &record : records) {
		if (ranges::contains(ids, record.id)) {
			if (record.randomId) {
				randomIds.emplace(record.randomId);
			}
			chat->byRandomId.remove(record.randomId);
			unregisterFile(record);
		}
	}
	auto placeholders = std::vector<FullMsgId>();
	chat->queue.erase(ranges::remove_if(chat->queue, [&](const auto &queued) {
		if (!queued || !randomIds.contains(queued->randomId)) {
			return false;
		}
		placeholders.push_back(queued->placeholder);
		return true;
	}), end(chat->queue));
	for (const auto placeholder : placeholders) {
		removePlaceholder(placeholder);
	}
	if (randomIds.contains(chat->sendingRandomId)) {
		_api.request(base::take(chat->sendRequestId)).cancel();
		chat->sending = false;
		chat->sendingRandomId = 0;
		chat->sendingPlaceholder = FullMsgId();
	}
	for (const auto randomId : randomIds) {
		replaceUnacked(chat, randomId);
	}
	const auto from = ranges::remove_if(records, [&](const HistoryRecord &r) {
		return ranges::contains(ids, r.id);
	});
	records.erase(from, end(records));
	saveHistory(chat);
	saveStates();
	sendNext(chat);
}

void Chats::replaceUnacked(not_null<Chat*> chat, uint64 randomId) {
	for (auto &message : chat->state.unacked) {
		if (message.randomId != randomId) {
			continue;
		}
		const auto layer = ParseLayer(bytes::make_span(message.serialized));
		if (!layer) {
			continue;
		}
		auto deleted = QVector<SecretTLlong>{
			secret_long(message.randomId),
		};
		const auto replacement = SecretTLDecryptedMessage(
			secret_decryptedMessageService(
				secret_long(message.randomId),
				secret_decryptedMessageActionDeleteMessages(
					secret_vector<SecretTLlong>(std::move(deleted)))));
		auto serialized = SerializeLayer(
			chat->state,
			replacement,
			layer->inSeqNo,
			message.outSeqNo);
		ClearBytes(message.serialized);
		message.serialized = std::move(serialized);
		message.file.reset();
		ClearBytes(message.pendingRecord);
		chat->pendingFileRetries.erase(ranges::remove(
			chat->pendingFileRetries,
			randomId), end(chat->pendingFileRetries));
		saveStates();
		flush();
		Storage::details::Sync();
		resendUnacked(chat, message);
	}
}

void Chats::updateUnreadCount(not_null<Chat*> chat) {
	const auto unread = ranges::count_if(
		chat->records,
		[](const HistoryRecord &r) { return !r.out && r.unread; });
	session().data().history(chat->peer)->setUnreadCount(int(unread));
}

void Chats::sendService(
		not_null<Chat*> chat,
		const SecretTLDecryptedMessageAction &action) {
	if (chat->state.state != SecretChatData::State::Ready
		|| chat->state.key.empty()) {
		DEBUG_LOG(("Secret Info: chat %1 has no key, the action stays local."
			).arg(chat->state.chatId));
		return;
	}
	enqueue(chat, MakeService(action));
}

void Chats::deleteMessages(
		not_null<SecretChatData*> peer,
		const std::vector<MsgId> &ids) {
	const auto chat = lookup(peer->secretChatId());
	if (!chat) {
		return;
	}
	auto randomIds = QVector<SecretTLlong>();
	auto dropped = std::vector<MsgId>();
	for (const auto &record : chat->records) {
		if (ranges::contains(ids, record.id)) {
			randomIds.push_back(secret_long(record.randomId));
			dropped.push_back(record.id);
		}
	}
	if (dropped.empty()) {
		return;
	}
	dropRecords(chat, dropped);
	updateUnreadCount(chat);
	sendService(chat, secret_decryptedMessageActionDeleteMessages(
		secret_vector<SecretTLlong>(std::move(randomIds))));
}

void Chats::deleteChat(not_null<SecretChatData*> peer, bool revoke) {
	const auto chat = lookup(peer->secretChatId());
	if (!chat) {
		return;
	}
	if (chat->state.state != SecretChatData::State::Closed) {
		sendDiscard(chat, revoke);
	}
	forgetChat(chat);
}

void Chats::forgetChat(not_null<Chat*> chat) {
	const auto peer = chat->peer;
	const auto id = peer->secretChatId();
	_api.request(base::take(chat->readRequestId)).cancel();
	_api.request(base::take(chat->typingRequestId)).cancel();
	const auto sendingPlaceholder = chat->sendingPlaceholder;
	chat->sending = false;
	chat->sendingRandomId = 0;
	chat->sendingPlaceholder = FullMsgId();
	_api.request(base::take(chat->sendRequestId)).cancel();
	clearQueue(chat);
	removePlaceholder(sendingPlaceholder);
	auto uploads = std::vector<FullMsgId>();
	for (const auto &[placeholder, upload] : _uploads) {
		if (upload.chatId == id) {
			uploads.push_back(placeholder);
		}
	}
	for (const auto placeholder : uploads) {
		_uploads.remove(placeholder);
		_session->uploader().cancel(placeholder);
		removePlaceholder(placeholder);
	}
	for (const auto &record : chat->records) {
		unregisterFile(record);
	}
	ClearKeyMaterial(chat->state);
	setChatState(chat, SecretChatData::State::Closed);
	auto pendingKey = takePendingKey(id);
	Crypto::ZeroAndClear(pendingKey);
	session().local().writeSecretHistory(peer->id, QByteArray());
	_chats.remove(id);
	saveStates();
}

void Chats::setTtl(not_null<SecretChatData*> peer, TimeId ttl) {
	const auto chat = lookup(peer->secretChatId());
	ttl = ClampTtl(ttl);
	if (!chat
		|| chat->state.state != SecretChatData::State::Ready
		|| chat->state.ttl == ttl) {
		return;
	}
	chat->state.ttl = ttl;
	peer->setTtl(ttl);
	saveStates();

	auto message = MakeService(secret_decryptedMessageActionSetMessageTTL(
		secret_int(ttl)));
	appendRecord(chat, HistoryRecord{
		.id = chat->state.nextMsgId++,
		.randomId = message->randomId,
		.out = true,
		.unread = true,
		.date = base::unixtime::now(),
		.serviceKind = ServiceKind::SetTtl,
		.servicePayload = QByteArray::number(ttl),
	});
	enqueue(chat, std::move(message));
}

void Chats::clearHistory(not_null<SecretChatData*> peer) {
	const auto chat = lookup(peer->secretChatId());
	if (!chat) {
		return;
	}
	dropRecords(chat, chat->records
		| ranges::views::transform(&HistoryRecord::id)
		| ranges::to_vector);
	updateUnreadCount(chat);
	sendService(chat, secret_decryptedMessageActionFlushHistory());
}

void Chats::answerKeyRequest(
		not_null<Chat*> chat,
		int64 exchangeId,
		bytes::vector &&gA) {
	auto &pfs = chat->state.pfs;
	if (pfs.stage == PfsState::Stage::Requested) {
		if (pfs.exchangeId > exchangeId) {
			DEBUG_LOG(("Secret Info: chat %1 keeps its own key exchange."
				).arg(chat->state.chatId));
			return;
		}
		enqueue(chat, MakeService(secret_decryptedMessageActionAbortKey(
			secret_long(pfs.exchangeId))));
		Crypto::ZeroAndClear(pfs.a);
		pfs.stage = PfsState::Stage::None;
	} else if (pfs.stage != PfsState::Stage::None
		|| chat->commitRandomId) {
		closeOnError(chat, u"requestKey arrived during an exchange"_q);
		return;
	}
	Crypto::ZeroAndClear(pfs.otherKey);
	pfs.otherFingerprint = 0;
	pfs.exchangeId = exchangeId;
	saveStates();

	const auto id = chat->peer->secretChatId();
	_dh.request([=, gA = std::move(gA)](const Crypto::DhConfig *config) {
		const auto entry = lookup(id);
		if (!entry
			|| entry->state.state != SecretChatData::State::Ready
			|| entry->state.pfs.stage != PfsState::Stage::None
			|| entry->state.pfs.exchangeId != exchangeId) {
			return;
		} else if (!config) {
			DEBUG_LOG(("Secret Error: chat %1 has no group to answer "
				"a new key in."
				).arg(entry->state.chatId));
			return;
		}
		auto answer = MakeAcceptorAnswer(*config, gA);
		if (answer.key.empty()) {
			closeOnError(entry, u"requestKey carried a bad g_a"_q);
			return;
		}
		auto &next = entry->state.pfs;
		next.stage = PfsState::Stage::Accepted;
		next.otherKey = std::move(answer.key);
		next.otherFingerprint = answer.keyFingerprint;
		saveStates();
		enqueue(entry, MakeService(secret_decryptedMessageActionAcceptKey(
			secret_long(exchangeId),
			secret_bytes(answer.gB),
			secret_long(answer.keyFingerprint))));
	});
}

void Chats::commitKey(
		not_null<Chat*> chat,
		int64 exchangeId,
		int64 fingerprint) {
	auto &pfs = chat->state.pfs;
	if (pfs.stage != PfsState::Stage::Accepted
		|| pfs.exchangeId != exchangeId
		|| pfs.otherFingerprint != fingerprint) {
		closeOnError(chat, u"commitKey does not match the exchange"_q);
		return;
	}
	std::swap(chat->state.key, pfs.otherKey);
	std::swap(chat->state.keyFingerprint, pfs.otherFingerprint);
	pfs.stage = PfsState::Stage::None;
	chat->state.keySetDate = base::unixtime::now();
	chat->state.messagesSinceKey = 0;
	DEBUG_LOG(("Secret Info: chat %1 switched to a new key."
		).arg(chat->state.chatId));
	saveStates();
}

void Chats::countMessage(not_null<Chat*> chat) {
	++chat->state.messagesSinceKey;
	const auto &pfs = chat->state.pfs;
	const auto now = base::unixtime::now();
	if (chat->state.state != SecretChatData::State::Ready
		|| chat->state.layerHis < kMinPfsLayer
		|| pfs.stage != PfsState::Stage::None
		|| !pfs.otherKey.empty()
		|| chat->commitRandomId
		|| (chat->state.messagesSinceKey < kMessagesPerKey
			&& now - chat->state.keySetDate < kKeyLifetime)) {
		return;
	}
	requestKey(chat);
}

void Chats::requestKey(not_null<Chat*> chat) {
	auto exchangeId = int64(base::RandomValue<uint64>() >> 1);
	while (!exchangeId) {
		exchangeId = int64(base::RandomValue<uint64>() >> 1);
	}
	chat->state.pfs.stage = PfsState::Stage::Requested;
	chat->state.pfs.exchangeId = exchangeId;
	saveStates();

	const auto id = chat->peer->secretChatId();
	_dh.request([=](const Crypto::DhConfig *config) {
		const auto entry = lookup(id);
		if (!entry
			|| entry->state.pfs.stage != PfsState::Stage::Requested
			|| entry->state.pfs.exchangeId != exchangeId) {
			return;
		}
		auto request = config
			? MakeCreatorRequest(*config)
			: CreatorRequest();
		if (entry->state.state != SecretChatData::State::Ready
			|| request.a.empty()) {
			DEBUG_LOG(("Secret Error: chat %1 cannot start a key exchange."
				).arg(entry->state.chatId));
			entry->state.pfs.stage = PfsState::Stage::None;
			saveStates();
			return;
		}
		entry->state.pfs.a = std::move(request.a);
		saveStates();
		DEBUG_LOG(("Secret Info: chat %1 asks for a new key."
			).arg(entry->state.chatId));
		enqueue(entry, MakeService(secret_decryptedMessageActionRequestKey(
			secret_long(exchangeId),
			secret_bytes(request.gA))));
	});
}

void Chats::acceptKey(
		not_null<Chat*> chat,
		int64 exchangeId,
		bytes::const_span gB,
		int64 fingerprint) {
	auto &pfs = chat->state.pfs;
	if (pfs.stage != PfsState::Stage::Requested
		|| pfs.exchangeId != exchangeId
		|| pfs.a.empty()) {
		closeOnError(chat, u"acceptKey does not match the exchange"_q);
		return;
	}
	const auto config = _dh.cached();
	if (!config) {
		const auto id = chat->peer->secretChatId();
		auto copied = bytes::make_vector(gB);
		_dh.request([=, gB = std::move(copied)](
				const Crypto::DhConfig *fetched) {
			const auto entry = lookup(id);
			if (!entry || !fetched) {
				return;
			}
			acceptKey(entry, exchangeId, gB, fingerprint);
		});
		return;
	}
	auto answer = MakeCreatorKey(*config, gB, pfs.a);
	Crypto::ZeroAndClear(pfs.a);
	if (answer.key.empty()
		|| answer.keyFingerprint != fingerprint
		|| answer.keyFingerprint == chat->state.keyFingerprint) {
		Crypto::ZeroAndClear(answer.key);
		closeOnError(chat, u"acceptKey carried a key we cannot use"_q);
		return;
	}
	pfs.stage = PfsState::Stage::Accepted;
	Crypto::ZeroAndClear(pfs.otherKey);
	pfs.otherKey = std::move(answer.key);
	pfs.otherFingerprint = answer.keyFingerprint;
	auto commit = MakeService(secret_decryptedMessageActionCommitKey(
		secret_long(exchangeId),
		secret_long(answer.keyFingerprint)));
	chat->commitRandomId = commit->randomId;
	saveStates();
	enqueue(chat, std::move(commit));
}

void Chats::abortKey(not_null<Chat*> chat, int64 exchangeId) {
	auto &pfs = chat->state.pfs;
	if (pfs.stage == PfsState::Stage::None
		|| pfs.exchangeId != exchangeId
		|| chat->commitRandomId) {
		return;
	}
	DEBUG_LOG(("Secret Info: chat %1 key exchange was aborted."
		).arg(chat->state.chatId));
	Crypto::ZeroAndClear(pfs.a);
	Crypto::ZeroAndClear(pfs.otherKey);
	pfs.otherFingerprint = 0;
	pfs.stage = PfsState::Stage::None;
	saveStates();
}

void Chats::switchToNewKey(not_null<Chat*> chat) {
	auto &pfs = chat->state.pfs;
	chat->commitRandomId = 0;
	std::swap(chat->state.key, pfs.otherKey);
	std::swap(chat->state.keyFingerprint, pfs.otherFingerprint);
	pfs.stage = PfsState::Stage::None;
	chat->state.keySetDate = base::unixtime::now();
	chat->state.messagesSinceKey = 0;
	DEBUG_LOG(("Secret Info: chat %1 committed a new key."
		).arg(chat->state.chatId));
}

void Chats::receiveText(
		not_null<Chat*> chat,
		uint64 randomId,
		TimeId date,
		int32 ttl,
		TextWithEntities &&text,
		uint64 replyToRandomId,
		const SecretTLDecryptedMessageMedia *media,
		const std::optional<EncryptedFileInfo> &file,
		uint64 groupedId) {
	if (!randomId || chat->byRandomId.contains(randomId)) {
		DEBUG_LOG(("Secret Info: chat %1 skipped a message it already has."
			).arg(chat->state.chatId));
		return;
	}
	auto mediaFile = media ? FileFromMedia(*media) : std::nullopt;
	const auto fileMatches = !mediaFile
		|| (file
			&& MediaFileSizeValid(*media, file)
			&& (Crypto::FileKeyFingerprint(mediaFile->key)
				== file->keyFingerprint));
	const auto stored = (media && fileMatches && IsStoredMedia(*media))
		? SerializeMedia(*media)
		: QByteArray();
	if (mediaFile) {
		Crypto::ZeroAndClear(mediaFile->key);
	}
	if (media) {
		auto details = QString();
		media->match([&](
				const SecretTLDdecryptedMessageMediaExternalDocument &data) {
			details = qs(data.vmime_type())
				+ ' '
				+ QString::number(data.vsize().v);
			for (const auto &attribute : data.vattributes().v) {
				details += ' ' + QString::number(attribute.type(), 16);
			}
		}, [](const auto &) {
		});
		DEBUG_LOG(("Secret Info: chat %1 got media %2 %3."
			).arg(chat->state.chatId
			).arg(QString::number(media->type(), 16)
			).arg(details));
	}
	if (!fileMatches) {
		DEBUG_LOG(("Secret Error: chat %1 got a file under another key."
			).arg(chat->state.chatId));
		addService(chat, randomId, date, ServiceKind::FileRejected);
		return;
	} else if (stored.isEmpty()
		&& media
		&& media->type() != secretc_decryptedMessageMediaEmpty) {
		DEBUG_LOG(("Secret Info: chat %1 got media, which is not shown yet."
			).arg(chat->state.chatId));
	}
	if (text.empty() && !stored.isEmpty()) {
		text = TextWithEntities{ CaptionFromMedia(*media) };
	}
	if (text.empty() && stored.isEmpty()) {
		return;
	}
	const auto replied = replyToRandomId
		? chat->byRandomId.find(replyToRandomId)
		: end(chat->byRandomId);
	const auto adjustedTtl = media
		? AdjustedMediaTtl(*media, ttl)
		: ClampTtl(ttl);
	appendRecord(chat, HistoryRecord{
		.id = chat->state.nextMsgId++,
		.randomId = randomId,
		.unread = true,
		.date = date,
		.ttl = adjustedTtl,
		.text = std::move(text),
		.replyToLocalId = ((replied != end(chat->byRandomId))
			? replied->second
			: MsgId(0)),
		.media = stored,
		.file = stored.isEmpty() ? std::nullopt : file,
		.groupedId = stored.isEmpty() ? uint64(0) : groupedId,
		.mediaUnread = !stored.isEmpty()
			&& MediaRequiresContentRead(*media, adjustedTtl),
	});
}

void Chats::handleTyping(SecretChatId id) {
	const auto entry = lookup(id);
	if (!entry) {
		return;
	}
	const auto user = entry->peer->user();
	if (!user) {
		return;
	}
	session().data().sendActionManager().registerFor(
		session().data().history(entry->peer),
		MsgId(0),
		user,
		MTP_sendMessageTypingAction(),
		base::unixtime::now());
}

void Chats::handleMessagesRead(SecretChatId id, TimeId maxDate) {
	const auto entry = lookup(id);
	if (!entry) {
		return;
	}
	auto tillId = MsgId(0);
	for (const auto &record : entry->records) {
		if (record.out && record.date <= maxDate) {
			accumulate_max(tillId, record.id);
		}
	}
	if (!tillId) {
		return;
	}
	auto changed = false;
	for (auto &record : entry->records) {
		if (!record.out || record.id > tillId) {
			continue;
		} else if (record.unread) {
			record.unread = false;
			changed = true;
		}
		if (record.ttl > 0 && !record.destroyAt && !record.mediaUnread) {
			record.destroyAt = DestroyAt(record.ttl);
			changed = true;
		}
	}
	if (changed) {
		saveHistory(entry);
		checkTtl();
	}
	session().data().history(entry->peer)->outboxRead(tillId);
}

void Chats::checkTtl() {
	const auto now = base::unixtime::now();
	auto next = TimeId(0);
	for (const auto &[id, chat] : _chats) {
		auto expired = std::vector<MsgId>();
		for (const auto &record : chat->records) {
			if (!record.destroyAt) {
				continue;
			} else if (record.destroyAt <= now) {
				expired.push_back(record.id);
			} else if (!next || record.destroyAt < next) {
				next = record.destroyAt;
			}
		}
		removeRecords(chat.get(), expired);
	}
	if (next) {
		_ttlTimer.callOnce((next - now) * crl::time(1000));
	} else {
		_ttlTimer.cancel();
	}
}

void Chats::readStates() {
	auto serialized = session().local().readSecretChats();
	const auto clear = gsl::finally([&] {
		if (serialized) {
			ClearBytes(*serialized);
		}
	});
	if (!serialized) {
		_statesWritable = false;
		DEBUG_LOG(("Secret Error: stored chat state could not be read."));
		return;
	}
	auto decoded = DeserializeChatStates(*serialized);
	if (!decoded) {
		_statesWritable = false;
		DEBUG_LOG(("Secret Error: stored chat state is corrupt."));
		return;
	}
	auto states = std::move(*decoded);
	DEBUG_LOG(("Secret Info: loaded %1 stored chats.").arg(states.size()));
	for (auto &state : states) {
		if (!state.key.empty()
			&& (int(state.key.size()) != Crypto::kKeySize
				|| Crypto::KeyFingerprint(state.key)
					!= state.keyFingerprint)) {
			ClearKeyMaterial(state);
			state.state = SecretChatData::State::Closed;
		} else if (state.keyHash.size() != Crypto::kKeyHashSize
			&& !state.key.empty()) {
			Crypto::ZeroAndClear(state.keyHash);
			state.keyHash = Crypto::KeyHashForVisualization(state.key);
		}
		if (!state.chatId || lookup(SecretChatIdFromServer(state.chatId))) {
			ClearKeyMaterial(state);
			continue;
		}
		RestoreUser(_session, state.user);
		const auto restored = emplace(std::move(state));
		DEBUG_LOG(("Secret Info: restored chat %1."
			).arg(restored->state.chatId));
	}
	checkTtl();
}

void Chats::writeStates() {
	if (!_statesWritable) {
		return;
	}
	auto states = std::vector<ChatState>();
	states.reserve(_chats.size());
	for (const auto &[id, chat] : _chats) {
		const auto user = session().data().userLoaded(chat->state.userId);
		if (user && user->isLoaded()) {
			chat->state.user = SerializeUser(user);
		}
		states.push_back(chat->state);
	}
	const auto clearing = gsl::finally([&] {
		for (auto &state : states) {
			ClearKeyMaterial(state);
		}
	});
	auto serialized = SerializeChatStates(states);
	if (!serialized) {
		DEBUG_LOG(("Secret Error: chat state is too large to store."));
		return;
	}
	const auto clearSerialized = gsl::finally([&] {
		ClearBytes(*serialized);
	});
	session().local().writeSecretChats(*serialized);
}

void Chats::readHistory(not_null<Chat*> chat) {
	auto serialized = session().local().readSecretHistory(chat->peer->id);
	const auto clear = gsl::finally([&] {
		if (serialized) {
			ClearBytes(*serialized);
		}
	});
	if (!serialized) {
		chat->historyWritable = false;
		DEBUG_LOG(("Secret Error: stored history for chat %1 could not be read."
			).arg(chat->state.chatId));
		return;
	}
	auto decoded = DeserializeHistory(*serialized);
	if (!decoded) {
		chat->historyWritable = false;
		DEBUG_LOG(("Secret Error: stored history for chat %1 is corrupt."
			).arg(chat->state.chatId));
		return;
	}
	auto records = std::move(*decoded);
	if (records.empty()) {
		return;
	}
	std::sort(
		begin(records),
		end(records),
		[](const HistoryRecord &a, const HistoryRecord &b) {
			return (a.id < b.id);
		});
	chat->records.reserve(records.size());
	for (auto &record : records) {
		if (!record.id
			|| (record.randomId
				&& (chat->byRandomId.find(record.randomId)
					!= end(chat->byRandomId)))) {
			continue;
		} else if (chat->state.nextMsgId <= record.id) {
			chat->state.nextMsgId = record.id + 1;
		}
		if (record.randomId) {
			chat->byRandomId.emplace(record.randomId, record.id);
		}
		registerFile(record);
		chat->records.push_back(std::move(record));
	}
	AddStoredHistory(chat->peer, chat->records);
	applyReadState(chat);
	DEBUG_LOG(("Secret Info: chat %1 loaded %2 stored messages."
		).arg(chat->state.chatId
		).arg(chat->records.size()));
}

void Chats::applyReadState(not_null<Chat*> chat) {
	auto inboxTill = MsgId(0);
	auto outboxTill = MsgId(0);
	auto inboxStopped = false;
	auto outboxStopped = false;
	auto unread = 0;
	for (const auto &record : chat->records) {
		auto &till = record.out ? outboxTill : inboxTill;
		auto &stopped = record.out ? outboxStopped : inboxStopped;
		if (record.unread) {
			stopped = true;
			unread += record.out ? 0 : 1;
		} else if (!stopped) {
			till = record.id;
		}
	}
	session().data().history(chat->peer)->applyDialogFields(
		nullptr,
		unread,
		inboxTill,
		outboxTill);
}

void Chats::saveHistory(not_null<Chat*> chat) {
	if (!chat->historyWritable) {
		return;
	}
	chat->historyChanged = true;
	if (!_historyTimer.isActive()) {
		_historyTimer.callOnce(kSaveDelay);
	}
}

void Chats::writeHistories() {
	for (const auto &[id, chat] : _chats) {
		if (!chat->historyChanged) {
			continue;
		}
		auto serialized = SerializeHistory(chat->records);
		if (!serialized) {
			DEBUG_LOG(("Secret Error: history for chat %1 is too large."
				).arg(chat->state.chatId));
			continue;
		}
		const auto clear = gsl::finally([&] { ClearBytes(*serialized); });
		chat->historyChanged = false;
		session().local().writeSecretHistory(
			chat->peer->id,
			*serialized);
	}
}

void Chats::sendReceivedQueue() {
	if (!_ackNeeded || !_pendingQts) {
		return;
	}
	_ackNeeded = false;
	flush();
	const auto maxQts = _pendingQts;
	DEBUG_LOG(("Secret Info: acknowledging the queue up to qts %1."
		).arg(maxQts));
	_api.request(MTPmessages_ReceivedQueue(
		MTP_int(maxQts)
	)).done([=](const MTPVector<MTPlong> &result) {
		DEBUG_LOG(("Secret Info: queue acknowledged up to qts %1, "
			"%2 ids returned."
			).arg(maxQts
			).arg(result.v.size()));
	}).fail([=](const MTP::Error &error) {
		DEBUG_LOG(("Secret Error: receivedQueue up to qts %1 failed, %2."
			).arg(maxQts
			).arg(error.type()));
	}).send();
}

} // namespace AyuSecret
