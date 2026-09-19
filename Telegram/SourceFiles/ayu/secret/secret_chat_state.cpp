// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/secret_chat_state.h"

#include "base/random.h"
#include "ui/text/text_entity.h"

#include <QtCore/QBuffer>
#include <QtCore/QDataStream>

#include <algorithm>

namespace AyuSecret {
namespace {

constexpr auto kChatStatesVersion = qint32(8);
constexpr auto kHistoryVersion = qint32(5);
constexpr auto kMaxChatCount = qint32(10'000);
constexpr auto kMaxHistoryCount = qint32(1'000'000);
constexpr auto kMaxPendingCount = qint32(100'000);
constexpr auto kMaxHeldCount = qint32(1'000);

void ClearBytes(bytes::vector &value) {
	if (!value.empty()) {
		base::RandomFill(value.data(), value.size());
		value.clear();
		value.shrink_to_fit();
	}
}

void ClearBytes(QByteArray &value) {
	if (!value.isEmpty()) {
		base::RandomFill(value.data(), value.size());
		value.clear();
		value.squeeze();
	}
}

void ClearState(ChatState &state) {
	ClearBytes(state.key);
	ClearBytes(state.keyHash);
	ClearBytes(state.a);
	ClearBytes(state.pfs.a);
	ClearBytes(state.pfs.otherKey);
	ClearBytes(state.gA);
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
	ClearBytes(state.user);
}

void ClearRecord(HistoryRecord &record) {
	ClearBytes(record.servicePayload);
	ClearBytes(record.media);
}

void WriteBytes(QDataStream &stream, const bytes::vector &value) {
	auto copy = QByteArray(
		reinterpret_cast<const char*>(value.data()),
		qsizetype(value.size()));
	stream << copy;
	ClearBytes(copy);
}

[[nodiscard]] bytes::vector ReadBytes(QDataStream &stream) {
	auto value = QByteArray();
	stream >> value;
	auto result = bytes::make_vector(value);
	ClearBytes(value);
	return result;
}

void WriteText(QDataStream &stream, const TextWithEntities &value) {
	const auto tags = TextUtilities::ConvertEntitiesToTextTags(
		value.entities);
	stream << value.text << TextUtilities::SerializeTags(tags);
}

[[nodiscard]] TextWithEntities ReadText(QDataStream &stream) {
	auto text = QString();
	auto tagsSerialized = QByteArray();
	stream >> text >> tagsSerialized;
	const auto tags = TextUtilities::DeserializeTags(
		tagsSerialized,
		int(text.size()));
	return TextWithEntities{
		text,
		TextUtilities::ConvertTextTagsToEntities(tags),
	};
}

[[nodiscard]] bool ReadCount(
		QDataStream &stream,
		qint32 &count,
		qint32 maximum) {
	stream >> count;
	return (stream.status() == QDataStream::Ok)
		&& (count >= 0)
		&& (count <= maximum);
}

[[nodiscard]] bool FitsCount(size_t count, qint32 maximum) {
	return count <= size_t(maximum);
}

[[nodiscard]] bool CanWriteState(const ChatState &state) {
	return FitsCount(state.unacked.size(), kMaxPendingCount)
		&& FitsCount(state.queuedFiles.size(), kMaxPendingCount)
		&& FitsCount(state.held.size(), kMaxHeldCount)
		&& FitsCount(state.services.size(), kMaxPendingCount);
}

void WritePfs(QDataStream &stream, const PfsState &pfs) {
	stream << qint32(pfs.stage) << qint64(pfs.exchangeId);
	WriteBytes(stream, pfs.a);
	WriteBytes(stream, pfs.otherKey);
	stream << qint64(pfs.otherFingerprint);
}

[[nodiscard]] PfsState ReadPfs(QDataStream &stream) {
	auto stage = qint32(0);
	auto exchangeId = qint64(0);
	stream >> stage >> exchangeId;
	auto result = PfsState();
	result.stage = (stage >= qint32(PfsState::Stage::None)
		&& stage <= qint32(PfsState::Stage::Accepted))
		? PfsState::Stage(stage)
		: PfsState::Stage::None;
	result.exchangeId = exchangeId;
	result.a = ReadBytes(stream);
	result.otherKey = ReadBytes(stream);
	auto otherFingerprint = qint64(0);
	stream >> otherFingerprint;
	result.otherFingerprint = otherFingerprint;
	return result;
}

void WriteFileInfo(QDataStream &stream, const EncryptedFileInfo &file) {
	stream
		<< qint64(file.id)
		<< qint64(file.accessHash)
		<< qint64(file.size)
		<< qint32(file.dcId)
		<< qint32(file.keyFingerprint);
}

[[nodiscard]] EncryptedFileInfo ReadFileInfo(QDataStream &stream) {
	auto id = qint64(0);
	auto accessHash = qint64(0);
	auto size = qint64(0);
	auto dcId = qint32(0);
	auto keyFingerprint = qint32(0);
	stream >> id >> accessHash >> size >> dcId >> keyFingerprint;
	return EncryptedFileInfo{
		.id = id,
		.accessHash = accessHash,
		.size = size,
		.dcId = int(dcId),
		.keyFingerprint = keyFingerprint,
	};
}

void WriteChatState(QDataStream &stream, const ChatState &state) {
	stream
		<< qint32(state.chatId)
		<< qint64(state.accessHash)
		<< quint64(state.userId.bare)
		<< qint32(state.creator ? 1 : 0)
		<< qint32(state.state)
		<< qint32(state.date)
		<< qint32(state.layerHis);
	WriteBytes(stream, state.key);
	stream
		<< qint64(state.keyFingerprint)
		<< qint32(state.keySetDate)
		<< qint32(state.messagesSinceKey);
	WriteBytes(stream, state.a);
	stream
		<< qint32(state.ttl)
		<< qint32(state.inCount)
		<< qint32(state.outCount)
		<< qint64(state.nextMsgId.bare);
	WritePfs(stream, state.pfs);
	stream << qint32(state.unacked.size());
	for (const auto &message : state.unacked) {
		stream
			<< qint32(message.outSeqNo)
			<< quint64(message.randomId)
			<< message.serialized
			<< qint32(message.file ? 1 : 0);
		if (const auto &file = message.file) {
			stream
				<< qint32(file->kind)
				<< qint64(file->id)
				<< qint32(file->parts)
				<< file->checksum
				<< qint32(file->fingerprint)
					<< qint64(file->accessHash);
		}
		stream << message.pendingRecord;
	}
	stream << qint32(state.held.size());
	for (const auto &message : state.held) {
		stream
			<< qint32(message.outSeqNo)
			<< message.decrypted
			<< qint32(message.date)
			<< qint32(message.file ? 1 : 0);
		if (message.file) {
			WriteFileInfo(stream, *message.file);
		}
	}
	stream << qint32(state.resendRequestedUpTo) << qint32(state.hisInSeqNo);
	WriteBytes(stream, state.gA);
	stream << qint32(state.services.size());
	for (const auto &service : state.services) {
		stream << quint64(service.randomId) << service.serialized;
	}
	stream << state.user;
	stream << qint32(state.queuedFiles.size());
	for (const auto &queued : state.queuedFiles) {
		stream << quint64(queued.randomId)
			<< queued.serialized
			<< qint32(queued.file.kind)
			<< qint64(queued.file.id)
			<< qint32(queued.file.parts)
			<< queued.file.checksum
			<< qint32(queued.file.fingerprint)
			<< qint64(queued.file.accessHash)
			<< queued.record
			<< qint32(queued.silent ? 1 : 0);
	}
}

[[nodiscard]] std::optional<ChatState> ReadChatState(
		QDataStream &stream,
		qint32 version) {
	auto chatId = qint32(0);
	auto accessHash = qint64(0);
	auto userId = quint64(0);
	auto creator = qint32(0);
	auto chatState = qint32(0);
	auto date = qint32(0);
	auto layerHis = qint32(0);
	stream
		>> chatId
		>> accessHash
		>> userId
		>> creator
		>> chatState
		>> date
		>> layerHis;

	auto result = ChatState();
	auto valid = false;
	const auto clear = gsl::finally([&] {
		if (!valid) {
			ClearState(result);
		}
	});
	result.chatId = chatId;
	result.accessHash = accessHash;
	result.userId = UserId(userId);
	result.creator = (creator == 1);
	result.state = (chatState >= qint32(SecretChatData::State::Requested)
		&& chatState <= qint32(SecretChatData::State::Closed))
		? SecretChatData::State(chatState)
		: SecretChatData::State::Closed;
	result.date = date;
	result.layerHis = layerHis;
	result.key = ReadBytes(stream);

	auto keyFingerprint = qint64(0);
	auto keySetDate = qint32(0);
	auto messagesSinceKey = qint32(0);
	stream >> keyFingerprint >> keySetDate >> messagesSinceKey;
	result.keyFingerprint = keyFingerprint;
	result.keySetDate = keySetDate;
	result.messagesSinceKey = messagesSinceKey;
	result.a = ReadBytes(stream);

	auto ttl = qint32(0);
	auto inCount = qint32(0);
	auto outCount = qint32(0);
	auto nextMsgId = qint64(0);
	stream >> ttl >> inCount >> outCount >> nextMsgId;
	if (messagesSinceKey < 0
		|| inCount < 0
		|| outCount < 0
		|| nextMsgId <= 0) {
		return std::nullopt;
	}
	result.ttl = std::clamp(ttl, int32(0), kMaxMessageTtl);
	result.inCount = inCount;
	result.outCount = outCount;
	result.nextMsgId = MsgId(nextMsgId);
	result.pfs = ReadPfs(stream);

	auto unackedCount = qint32(0);
	if (!ReadCount(stream, unackedCount, kMaxPendingCount)) {
		return std::nullopt;
	}
	result.unacked.reserve(unackedCount);
	for (auto i = 0; i != unackedCount; ++i) {
		auto outSeqNo = qint32(0);
		auto randomId = quint64(0);
		auto serialized = QByteArray();
		const auto clearSerialized = gsl::finally([&] {
			ClearBytes(serialized);
		});
		stream >> outSeqNo >> randomId >> serialized;
		if (stream.status() != QDataStream::Ok) {
			return std::nullopt;
		}
		auto file = std::optional<OutgoingFile>();
		if (version >= 6) {
			auto present = qint32(0);
			stream >> present;
			if (present == 1) {
				auto kind = qint32(0);
				auto id = qint64(0);
				auto parts = qint32(0);
				auto checksum = QString();
				auto fingerprint = qint32(0);
				auto accessHash = qint64(0);
				stream >> kind >> id >> parts >> checksum
					>> fingerprint >> accessHash;
				if (kind < qint32(OutgoingFile::Kind::Uploaded)
					|| kind > qint32(OutgoingFile::Kind::Existing)
					|| id == 0
					|| (kind != qint32(OutgoingFile::Kind::Existing)
						&& parts <= 0)) {
					return std::nullopt;
				}
				file = OutgoingFile{
					.kind = OutgoingFile::Kind(kind),
					.id = id,
					.parts = parts,
					.checksum = std::move(checksum),
					.fingerprint = fingerprint,
					.accessHash = accessHash,
				};
			} else if (present != 0) {
				return std::nullopt;
			}
		}
		if (stream.status() != QDataStream::Ok) {
			return std::nullopt;
		}
		auto pendingRecord = QByteArray();
		if (version >= 7) {
			stream >> pendingRecord;
			if (stream.status() != QDataStream::Ok) {
				return std::nullopt;
			}
		}
		result.unacked.push_back({
			.outSeqNo = outSeqNo,
			.randomId = randomId,
			.serialized = std::move(serialized),
			.file = std::move(file),
			.pendingRecord = std::move(pendingRecord),
		});
	}

	auto heldCount = qint32(0);
	if (!ReadCount(stream, heldCount, kMaxHeldCount)) {
		return std::nullopt;
	}
	result.held.reserve(heldCount);
	for (auto i = 0; i != heldCount; ++i) {
		auto outSeqNo = qint32(0);
		auto decrypted = QByteArray();
		const auto clearDecrypted = gsl::finally([&] {
			ClearBytes(decrypted);
		});
		auto heldDate = qint32(0);
		auto filePresent = qint32(0);
		stream >> outSeqNo >> decrypted >> heldDate >> filePresent;
		auto file = std::optional<EncryptedFileInfo>();
		if (filePresent == 1) {
			file = ReadFileInfo(stream);
			if (file->size <= 0) {
				return std::nullopt;
			}
		}
		if (stream.status() != QDataStream::Ok) {
			return std::nullopt;
		}
		result.held.push_back({
			.outSeqNo = outSeqNo,
			.decrypted = std::move(decrypted),
			.date = heldDate,
			.file = file,
		});
	}

	auto resendRequestedUpTo = qint32(0);
	stream >> resendRequestedUpTo;
	result.resendRequestedUpTo = resendRequestedUpTo;
	if (version >= 2) {
		auto hisInSeqNo = qint32(0);
		stream >> hisInSeqNo;
		result.hisInSeqNo = hisInSeqNo;
	}
	if (version >= 3) {
		result.gA = ReadBytes(stream);
	}
	if (version >= 4) {
		auto servicesCount = qint32(0);
		if (!ReadCount(stream, servicesCount, kMaxPendingCount)) {
			return std::nullopt;
		}
		result.services.reserve(servicesCount);
		for (auto i = 0; i != servicesCount; ++i) {
			auto randomId = quint64(0);
			auto serialized = QByteArray();
			const auto clearSerialized = gsl::finally([&] {
				ClearBytes(serialized);
			});
			stream >> randomId >> serialized;
			if (stream.status() != QDataStream::Ok) {
				return std::nullopt;
			}
			result.services.push_back({
				.randomId = randomId,
				.serialized = std::move(serialized),
			});
		}
	}
	if (version >= 5) {
		stream >> result.user;
	}
	if (version >= 8) {
		auto count = qint32(0);
		if (!ReadCount(stream, count, kMaxPendingCount)) {
			return std::nullopt;
		}
		result.queuedFiles.reserve(count);
		for (auto i = 0; i != count; ++i) {
			auto queued = QueuedFile();
			auto kind = qint32(0);
			auto silent = qint32(0);
			stream >> queued.randomId >> queued.serialized
				>> kind >> queued.file.id >> queued.file.parts
				>> queued.file.checksum >> queued.file.fingerprint
				>> queued.file.accessHash >> queued.record >> silent;
			if (stream.status() != QDataStream::Ok
				|| !queued.randomId
				|| kind < qint32(OutgoingFile::Kind::Uploaded)
				|| kind > qint32(OutgoingFile::Kind::Existing)
				|| !queued.file.id
				|| (kind != qint32(OutgoingFile::Kind::Existing)
					&& queued.file.parts <= 0)
				|| (silent != 0 && silent != 1)) {
				ClearBytes(queued.serialized);
				ClearBytes(queued.record);
				return std::nullopt;
			}
			queued.file.kind = OutgoingFile::Kind(kind);
			queued.silent = (silent == 1);
			result.queuedFiles.push_back(std::move(queued));
		}
	}
	if (stream.status() != QDataStream::Ok) {
		return std::nullopt;
	}
	valid = true;
	return std::make_optional(std::move(result));
}

void WriteHistoryRecord(QDataStream &stream, const HistoryRecord &record) {
	stream
		<< qint64(record.id.bare)
		<< quint64(record.randomId)
		<< qint32(record.out ? 1 : 0)
		<< qint32(record.unread ? 1 : 0)
		<< qint32(record.date)
		<< qint32(record.ttl)
		<< qint32(record.destroyAt);
	WriteText(stream, record.text);
	stream
		<< qint64(record.replyToLocalId.bare)
		<< qint32(record.serviceKind)
		<< record.servicePayload
		<< record.media
		<< qint32(record.file ? 1 : 0);
	if (record.file) {
		WriteFileInfo(stream, *record.file);
	}
	stream
		<< qint32(record.unsent ? 1 : 0)
		<< quint64(record.groupedId)
		<< qint32(record.silent ? 1 : 0);
}

[[nodiscard]] std::optional<HistoryRecord> ReadHistoryRecord(
		QDataStream &stream,
		qint32 version) {
	auto id = qint64(0);
	auto randomId = quint64(0);
	auto out = qint32(0);
	auto unread = qint32(0);
	auto date = qint32(0);
	auto ttl = qint32(0);
	auto destroyAt = qint32(0);
	stream >> id >> randomId >> out >> unread >> date >> ttl >> destroyAt;

	auto result = HistoryRecord();
	auto valid = false;
	const auto clear = gsl::finally([&] {
		if (!valid) {
			ClearRecord(result);
		}
	});
	result.id = MsgId(id);
	result.randomId = randomId;
	result.out = (out == 1);
	result.unread = (unread == 1);
	result.date = date;
	result.ttl = std::clamp(ttl, int32(0), kMaxMessageTtl);
	result.destroyAt = destroyAt;
	result.text = ReadText(stream);

	auto replyToLocalId = qint64(0);
	auto serviceKind = qint32(0);
	auto servicePayload = QByteArray();
	stream >> replyToLocalId >> serviceKind >> servicePayload;
	result.replyToLocalId = MsgId(replyToLocalId);
	result.serviceKind = (serviceKind >= qint32(ServiceKind::None)
		&& serviceKind <= qint32(ServiceKind::FileRejected))
		? ServiceKind(serviceKind)
		: ServiceKind::None;
	result.servicePayload = std::move(servicePayload);
	if (version >= 2) {
		auto hasFile = qint32(0);
		stream >> result.media >> hasFile;
		if (hasFile == 1) {
			result.file = ReadFileInfo(stream);
			if (result.file->size <= 0) {
				return std::nullopt;
			}
		}
	}
	if (version >= 3) {
		auto unsent = qint32(0);
		stream >> unsent;
		result.unsent = (unsent == 1);
	}
	if (version >= 4) {
		auto groupedId = quint64(0);
		stream >> groupedId;
		result.groupedId = groupedId;
	}
	if (version >= 5) {
		auto silent = qint32(0);
		stream >> silent;
		result.silent = (silent == 1);
	}
	if (stream.status() != QDataStream::Ok) {
		return std::nullopt;
	}
	valid = true;
	return std::make_optional(std::move(result));
}

} // namespace

std::optional<QByteArray> SerializeChatStates(
		const std::vector<ChatState> &states) {
	if (states.empty()) {
		return QByteArray();
	}
	if (!FitsCount(states.size(), kMaxChatCount)
		|| !std::all_of(states.begin(), states.end(), CanWriteState)) {
		return std::nullopt;
	}
	auto result = QByteArray();
	{
		QBuffer buffer(&result);
		buffer.open(QIODevice::WriteOnly);
		QDataStream stream(&buffer);
		stream.setVersion(QDataStream::Qt_5_1);
		stream << kChatStatesVersion << qint32(states.size());
		for (const auto &state : states) {
			WriteChatState(stream, state);
		}
		for (const auto &state : states) {
			WriteBytes(stream, state.keyHash);
		}
	}
	return result;
}

std::optional<std::vector<ChatState>> DeserializeChatStates(
		const QByteArray &serialized) {
	if (serialized.isEmpty()) {
		return std::vector<ChatState>();
	}
	QDataStream stream(serialized);
	stream.setVersion(QDataStream::Qt_5_1);

	auto version = qint32(0);
	stream >> version;
	if (stream.status() != QDataStream::Ok
		|| version < 1
		|| version > kChatStatesVersion) {
		return std::nullopt;
	}
	auto count = qint32(0);
	if (!ReadCount(stream, count, kMaxChatCount)) {
		return std::nullopt;
	}
	auto result = std::vector<ChatState>();
	auto valid = false;
	const auto clear = gsl::finally([&] {
		if (!valid) {
			for (auto &state : result) {
				ClearState(state);
			}
		}
	});
	result.reserve(count);
	for (auto i = 0; i != count; ++i) {
		auto state = ReadChatState(stream, version);
		if (!state) {
			return std::nullopt;
		}
		result.push_back(std::move(*state));
	}
	if (!stream.atEnd()) {
		for (auto &state : result) {
			state.keyHash = ReadBytes(stream);
		}
	}
	if (stream.status() != QDataStream::Ok || !stream.atEnd()) {
		return std::nullopt;
	}
	valid = true;
	return std::make_optional(std::move(result));
}

std::optional<QByteArray> SerializeHistory(
		const std::vector<HistoryRecord> &records) {
	if (records.empty()) {
		return QByteArray();
	}
	if (!FitsCount(records.size(), kMaxHistoryCount)) {
		return std::nullopt;
	}
	auto result = QByteArray();
	{
		QBuffer buffer(&result);
		buffer.open(QIODevice::WriteOnly);
		QDataStream stream(&buffer);
		stream.setVersion(QDataStream::Qt_5_1);
		stream << kHistoryVersion << qint32(records.size());
		for (const auto &record : records) {
			WriteHistoryRecord(stream, record);
		}
		for (const auto &record : records) {
			stream << qint32(record.mediaUnread ? 1 : 0);
		}
	}
	return result;
}

std::optional<std::vector<HistoryRecord>> DeserializeHistory(
		const QByteArray &serialized) {
	if (serialized.isEmpty()) {
		return std::vector<HistoryRecord>();
	}
	QDataStream stream(serialized);
	stream.setVersion(QDataStream::Qt_5_1);

	auto version = qint32(0);
	stream >> version;
	if (stream.status() != QDataStream::Ok
		|| version < 1
		|| version > kHistoryVersion) {
		return std::nullopt;
	}
	auto count = qint32(0);
	if (!ReadCount(stream, count, kMaxHistoryCount)) {
		return std::nullopt;
	}
	auto result = std::vector<HistoryRecord>();
	auto valid = false;
	const auto clear = gsl::finally([&] {
		if (!valid) {
			for (auto &record : result) {
				ClearRecord(record);
			}
		}
	});
	result.reserve(count);
	for (auto i = 0; i != count; ++i) {
		auto record = ReadHistoryRecord(stream, version);
		if (!record) {
			return std::nullopt;
		}
		result.push_back(std::move(*record));
	}
	if (!stream.atEnd()) {
		for (auto &record : result) {
			auto mediaUnread = qint32(0);
			stream >> mediaUnread;
			if (mediaUnread != 0 && mediaUnread != 1) {
				return std::nullopt;
			}
			record.mediaUnread = (mediaUnread == 1);
		}
	}
	if (stream.status() != QDataStream::Ok || !stream.atEnd()) {
		return std::nullopt;
	}
	valid = true;
	return std::make_optional(std::move(result));
}

} // namespace AyuSecret
