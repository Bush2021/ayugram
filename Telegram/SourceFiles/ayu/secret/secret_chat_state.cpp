// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/secret_chat_state.h"

#include "ui/text/text_entity.h"

#include <QtCore/QBuffer>
#include <QtCore/QDataStream>

namespace AyuSecret {
namespace {

constexpr auto kChatStatesVersion = qint32(5);
constexpr auto kHistoryVersion = qint32(4);
constexpr auto kMaxCount = qint32(1'000'000);

void WriteBytes(QDataStream &stream, const bytes::vector &value) {
	stream << QByteArray(
		reinterpret_cast<const char*>(value.data()),
		qsizetype(value.size()));
}

[[nodiscard]] bytes::vector ReadBytes(QDataStream &stream) {
	auto value = QByteArray();
	stream >> value;
	return bytes::make_vector(value);
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

[[nodiscard]] bool ReadCount(QDataStream &stream, qint32 &count) {
	stream >> count;
	return (stream.status() == QDataStream::Ok)
		&& (count >= 0)
		&& (count <= kMaxCount);
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
			<< message.serialized;
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
	result.ttl = ttl;
	result.inCount = inCount;
	result.outCount = outCount;
	result.nextMsgId = MsgId(nextMsgId);
	result.pfs = ReadPfs(stream);

	auto unackedCount = qint32(0);
	if (!ReadCount(stream, unackedCount)) {
		return std::nullopt;
	}
	result.unacked.reserve(unackedCount);
	for (auto i = 0; i != unackedCount; ++i) {
		auto outSeqNo = qint32(0);
		auto randomId = quint64(0);
		auto serialized = QByteArray();
		stream >> outSeqNo >> randomId >> serialized;
		if (stream.status() != QDataStream::Ok) {
			return std::nullopt;
		}
		result.unacked.push_back({
			.outSeqNo = outSeqNo,
			.randomId = randomId,
			.serialized = serialized,
		});
	}

	auto heldCount = qint32(0);
	if (!ReadCount(stream, heldCount)) {
		return std::nullopt;
	}
	result.held.reserve(heldCount);
	for (auto i = 0; i != heldCount; ++i) {
		auto outSeqNo = qint32(0);
		auto decrypted = QByteArray();
		auto heldDate = qint32(0);
		auto filePresent = qint32(0);
		stream >> outSeqNo >> decrypted >> heldDate >> filePresent;
		auto file = std::optional<EncryptedFileInfo>();
		if (filePresent == 1) {
			file = ReadFileInfo(stream);
		}
		if (stream.status() != QDataStream::Ok) {
			return std::nullopt;
		}
		result.held.push_back({
			.outSeqNo = outSeqNo,
			.decrypted = decrypted,
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
		if (!ReadCount(stream, servicesCount)) {
			return std::nullopt;
		}
		result.services.reserve(servicesCount);
		for (auto i = 0; i != servicesCount; ++i) {
			auto randomId = quint64(0);
			auto serialized = QByteArray();
			stream >> randomId >> serialized;
			if (stream.status() != QDataStream::Ok) {
				return std::nullopt;
			}
			result.services.push_back({
				.randomId = randomId,
				.serialized = serialized,
			});
		}
	}
	if (version >= 5) {
		stream >> result.user;
	}
	if (stream.status() != QDataStream::Ok) {
		return std::nullopt;
	}
	return result;
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
	stream << qint32(record.unsent ? 1 : 0) << quint64(record.groupedId);
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
	result.id = MsgId(id);
	result.randomId = randomId;
	result.out = (out == 1);
	result.unread = (unread == 1);
	result.date = date;
	result.ttl = ttl;
	result.destroyAt = destroyAt;
	result.text = ReadText(stream);

	auto replyToLocalId = qint64(0);
	auto serviceKind = qint32(0);
	auto servicePayload = QByteArray();
	stream >> replyToLocalId >> serviceKind >> servicePayload;
	result.replyToLocalId = MsgId(replyToLocalId);
	result.serviceKind = ServiceKind(serviceKind);
	result.servicePayload = servicePayload;
	if (version >= 2) {
		auto hasFile = qint32(0);
		stream >> result.media >> hasFile;
		if (hasFile == 1) {
			result.file = ReadFileInfo(stream);
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
	if (stream.status() != QDataStream::Ok) {
		return std::nullopt;
	}
	return result;
}

} // namespace

QByteArray SerializeChatStates(const std::vector<ChatState> &states) {
	if (states.empty()) {
		return QByteArray();
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
	}
	return result;
}

std::vector<ChatState> DeserializeChatStates(const QByteArray &serialized) {
	QDataStream stream(serialized);
	stream.setVersion(QDataStream::Qt_5_1);

	auto version = qint32(0);
	stream >> version;
	if (stream.status() != QDataStream::Ok
		|| version < 1
		|| version > kChatStatesVersion) {
		return {};
	}
	auto count = qint32(0);
	if (!ReadCount(stream, count)) {
		return {};
	}
	auto result = std::vector<ChatState>();
	result.reserve(count);
	for (auto i = 0; i != count; ++i) {
		auto state = ReadChatState(stream, version);
		if (!state) {
			return {};
		}
		result.push_back(std::move(*state));
	}
	return result;
}

QByteArray SerializeHistory(const std::vector<HistoryRecord> &records) {
	if (records.empty()) {
		return QByteArray();
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
	}
	return result;
}

std::vector<HistoryRecord> DeserializeHistory(const QByteArray &serialized) {
	QDataStream stream(serialized);
	stream.setVersion(QDataStream::Qt_5_1);

	auto version = qint32(0);
	stream >> version;
	if (stream.status() != QDataStream::Ok
		|| version < 1
		|| version > kHistoryVersion) {
		return {};
	}
	auto count = qint32(0);
	if (!ReadCount(stream, count)) {
		return {};
	}
	auto result = std::vector<HistoryRecord>();
	result.reserve(count);
	for (auto i = 0; i != count; ++i) {
		auto record = ReadHistoryRecord(stream, version);
		if (!record) {
			return {};
		}
		result.push_back(std::move(*record));
	}
	return result;
}

} // namespace AyuSecret
