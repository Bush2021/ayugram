// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/secret_media.h"

#include "ayu/secret/secret_chat_state.h"
#include "ayu/secret/secret_chats.h"
#include "data/data_document.h"
#include "data/data_session.h"
#include "data/stickers/data_stickers.h"
#include "data/stickers/data_stickers_set.h"
#include "main/main_session.h"
#include "storage/file_download.h"
#include "storage/localimageloader.h"

#include <QtCore/QBuffer>

#include <algorithm>
#include <limits>

namespace AyuSecret {
namespace {

constexpr auto kThumbSide = 90;
constexpr auto kThumbQuality = 87;
constexpr auto kLongSizeLayer = 143;
constexpr auto kContentReadTtlLimit = int32(60);

struct PreparedThumb {
	QByteArray bytes;
	int w = 0;
	int h = 0;
};

template <typename Attributes>
[[nodiscard]] int MediaDuration(const Attributes &attributes) {
	auto result = 0;
	for (const auto &attribute : attributes.v) {
		attribute.match([&](const SecretTLDdocumentAttributeVideo23 &data) {
			accumulate_max(result, data.vduration().v);
		}, [&](const SecretTLDdocumentAttributeVideo &data) {
			accumulate_max(result, data.vduration().v);
		}, [&](const SecretTLDdocumentAttributeAudio23 &data) {
			accumulate_max(result, data.vduration().v);
		}, [&](const SecretTLDdocumentAttributeAudio45 &data) {
			accumulate_max(result, data.vduration().v);
		}, [&](const SecretTLDdocumentAttributeAudio &data) {
			accumulate_max(result, data.vduration().v);
		}, [](const auto &) {
		});
	}
	return result;
}

template <typename Attributes>
[[nodiscard]] bool HasTimedAttribute(const Attributes &attributes) {
	return ranges::any_of(attributes.v, [](const auto &attribute) {
		switch (attribute.type()) {
		case secretc_documentAttributeVideo23:
		case secretc_documentAttributeVideo:
		case secretc_documentAttributeAudio23:
		case secretc_documentAttributeAudio45:
		case secretc_documentAttributeAudio:
			return true;
		}
		return false;
	});
}

[[nodiscard]] int MediaDuration(
		const SecretTLDecryptedMessageMedia &media) {
	return media.match([](const SecretTLDdecryptedMessageMediaVideo8 &data) {
		return data.vduration().v;
	}, [](const SecretTLDdecryptedMessageMediaVideo23 &data) {
		return data.vduration().v;
	}, [](const SecretTLDdecryptedMessageMediaVideo &data) {
		return data.vduration().v;
	}, [](const SecretTLDdecryptedMessageMediaAudio8 &data) {
		return data.vduration().v;
	}, [](const SecretTLDdecryptedMessageMediaAudio &data) {
		return data.vduration().v;
	}, [](const SecretTLDdecryptedMessageMediaDocument46 &data) {
		return MediaDuration(data.vattributes());
	}, [](const SecretTLDdecryptedMessageMediaDocument &data) {
		return MediaDuration(data.vattributes());
	}, [](const auto &) {
		return 0;
	});
}

[[nodiscard]] bool IsTimedMedia(
		const SecretTLDecryptedMessageMedia &media) {
	switch (media.type()) {
	case secretc_decryptedMessageMediaPhoto8:
	case secretc_decryptedMessageMediaPhoto:
	case secretc_decryptedMessageMediaVideo8:
	case secretc_decryptedMessageMediaVideo23:
	case secretc_decryptedMessageMediaVideo:
	case secretc_decryptedMessageMediaAudio8:
	case secretc_decryptedMessageMediaAudio:
		return true;
	}
	return media.match([](
			const SecretTLDdecryptedMessageMediaDocument46 &data) {
		return HasTimedAttribute(data.vattributes());
	}, [](const SecretTLDdecryptedMessageMediaDocument &data) {
		return HasTimedAttribute(data.vattributes());
	}, [](const auto &) {
		return false;
	});
}

[[nodiscard]] MTPInputStickerSet StickerSet(
		const SecretTLInputStickerSet &set) {
	return set.match([](const SecretTLDinputStickerSetShortName &data) {
		return MTP_inputStickerSetShortName(data.vshort_name());
	}, [](const SecretTLDinputStickerSetEmpty &) {
		return MTP_inputStickerSetEmpty();
	});
}

[[nodiscard]] MTPDocumentAttribute Sticker(
		const MTPstring &alt,
		const MTPInputStickerSet &set) {
	return MTP_documentAttributeSticker(
		MTP_flags(MTPDdocumentAttributeSticker::Flag(0)),
		alt,
		set,
		MTPMaskCoords());
}

[[nodiscard]] MTPDocumentAttribute Video(
		bool round,
		int duration,
		const MTPint &w,
		const MTPint &h) {
	using Flag = MTPDdocumentAttributeVideo::Flag;
	return MTP_documentAttributeVideo(
		MTP_flags(round ? Flag::f_round_message : Flag(0)),
		MTP_double(duration),
		w,
		h,
		MTPint(), // preload_prefix_size
		MTPdouble(), // video_start_ts
		MTPstring()); // video_codec
}

[[nodiscard]] MTPDocumentAttribute Audio(
		bool voice,
		int duration,
		const MTPstring *title,
		const MTPstring *performer,
		const MTPbytes *waveform) {
	using Flag = MTPDdocumentAttributeAudio::Flag;
	return MTP_documentAttributeAudio(
		MTP_flags(Flag(0)
			| (voice ? Flag::f_voice : Flag(0))
			| (title ? Flag::f_title : Flag(0))
			| (performer ? Flag::f_performer : Flag(0))
			| (waveform ? Flag::f_waveform : Flag(0))),
		MTP_int(duration),
		title ? *title : MTPstring(),
		performer ? *performer : MTPstring(),
		waveform ? *waveform : MTPbytes());
}

[[nodiscard]] QVector<MTPDocumentAttribute> Attributes(
		const QVector<SecretTLDocumentAttribute> &list) {
	auto result = QVector<MTPDocumentAttribute>();
	result.reserve(list.size());
	for (const auto &attribute : list) {
		attribute.match([&](const SecretTLDdocumentAttributeImageSize &d) {
			result.push_back(MTP_documentAttributeImageSize(d.vw(), d.vh()));
		}, [&](const SecretTLDdocumentAttributeAnimated &) {
			result.push_back(MTP_documentAttributeAnimated());
		}, [&](const SecretTLDdocumentAttributeSticker23 &) {
			result.push_back(
				Sticker(MTP_string(), MTP_inputStickerSetEmpty()));
		}, [&](const SecretTLDdocumentAttributeSticker &d) {
			result.push_back(Sticker(d.valt(), StickerSet(d.vstickerset())));
		}, [&](const SecretTLDdocumentAttributeVideo23 &d) {
			result.push_back(Video(false, d.vduration().v, d.vw(), d.vh()));
		}, [&](const SecretTLDdocumentAttributeVideo &d) {
			result.push_back(Video(
				d.is_round_message(),
				d.vduration().v,
				d.vw(),
				d.vh()));
		}, [&](const SecretTLDdocumentAttributeAudio23 &d) {
			result.push_back(
				Audio(false, d.vduration().v, nullptr, nullptr, nullptr));
		}, [&](const SecretTLDdocumentAttributeAudio45 &d) {
			result.push_back(Audio(
				false,
				d.vduration().v,
				&d.vtitle(),
				&d.vperformer(),
				nullptr));
		}, [&](const SecretTLDdocumentAttributeAudio &d) {
			result.push_back(Audio(
				d.is_voice(),
				d.vduration().v,
				d.vtitle(),
				d.vperformer(),
				d.vwaveform()));
		}, [&](const SecretTLDdocumentAttributeFilename &d) {
			result.push_back(MTP_documentAttributeFilename(d.vfile_name()));
		});
	}
	return result;
}

[[nodiscard]] MTPMessageMedia ExternalDocument(
		not_null<Main::Session*> session,
		const SecretTLDdecryptedMessageMediaExternalDocument &data) {
	// WHY: a sticker already known here keeps its fields, the empty file
	// reference and the short-name set of this copy would overwrite them,
	// and a zero date makes the document skip every field.
	const auto known = (session->data().document(data.vid().v)->date != 0);
	auto thumbs = QVector<MTPPhotoSize>();
	data.vthumb().match([&](const SecretTLDphotoCachedSize &d) {
		thumbs.push_back(
			MTP_photoCachedSize(d.vtype(), d.vw(), d.vh(), d.vbytes()));
	}, [](const auto &) {
	});
	using Flag = MTPDdocument::Flag;
	const auto document = MTP_document(
		MTP_flags(thumbs.isEmpty() ? Flag(0) : Flag::f_thumbs),
		data.vid(),
		data.vaccess_hash(),
		MTP_bytes(), // file_reference
		MTP_int(known ? 0 : data.vdate().v),
		data.vmime_type(),
		MTP_long(data.vsize().v),
		MTP_vector<MTPPhotoSize>(std::move(thumbs)),
		MTPVector<MTPVideoSize>(),
		data.vdc_id(),
		MTP_vector<MTPDocumentAttribute>(Attributes(data.vattributes().v)));
	return MTP_messageMediaDocument(
		MTP_flags(MTPDmessageMediaDocument::Flag::f_document),
		document,
		MTPVector<MTPDocument>(), // alt_documents
		MTPPhoto(), // video_cover
		MTPint(), // video_timestamp
		MTPint()); // ttl_seconds
}

template <typename Data>
[[nodiscard]] MediaFile FileOf(const Data &data) {
	return MediaFile{
		.key = Crypto::FileKey{
			.key = bytes::make_vector(data.vkey().v),
			.iv = bytes::make_vector(data.viv().v),
		},
		.size = int64(data.vsize().v),
	};
}

[[nodiscard]] QVector<MTPPhotoSize> Thumb(
		const MTPbytes &bytes,
		const MTPint &w,
		const MTPint &h) {
	if (bytes.v.isEmpty()) {
		return {};
	}
	return { MTP_photoCachedSize(MTP_string("s"), w, h, bytes) };
}

[[nodiscard]] MTPMessageMedia Photo(
		const HistoryRecord &record,
		const EncryptedFileInfo &file,
		int32 ttl,
		QVector<MTPPhotoSize> &&sizes,
		const MTPint &w,
		const MTPint &h,
		const MTPint &size) {
	sizes.push_back(MTP_photoSize(MTP_string("y"), w, h, size));
	return MTP_messageMediaPhoto(
		MTP_flags(MTPDmessageMediaPhoto::Flag::f_photo
			| (ttl
				? MTPDmessageMediaPhoto::Flag::f_ttl_seconds
				: MTPDmessageMediaPhoto::Flag(0))),
		MTP_photo(
			MTP_flags(MTPDphoto::Flag(0)),
			MTP_long(file.id),
			MTP_long(file.accessHash),
			MTP_bytes(), // file_reference
			MTP_int(record.date),
			MTP_vector<MTPPhotoSize>(std::move(sizes)),
			MTPVector<MTPVideoSize>(),
			MTP_int(file.dcId)),
		ttl ? MTP_int(ttl) : MTPint(),
		MTPDocument()); // video
}

[[nodiscard]] MTPMessageMedia Document(
		const HistoryRecord &record,
		const EncryptedFileInfo &file,
		int32 ttl,
		QVector<MTPPhotoSize> &&thumbs,
		const MTPstring &mime,
		int64 size,
		QVector<MTPDocumentAttribute> &&attributes) {
	using Flag = MTPDdocument::Flag;
	const auto flags = thumbs.isEmpty() ? Flag(0) : Flag::f_thumbs;
	const auto boundedSize = std::clamp(
		size,
		int64(0),
		std::max(file.size, int64(0)));
	return MTP_messageMediaDocument(
		MTP_flags(MTPDmessageMediaDocument::Flag::f_document
			| (ttl
				? MTPDmessageMediaDocument::Flag::f_ttl_seconds
				: MTPDmessageMediaDocument::Flag(0))),
		MTP_document(
			MTP_flags(flags),
			MTP_long(file.id),
			MTP_long(file.accessHash),
			MTP_bytes(), // file_reference
			MTP_int(record.date),
			mime,
			MTP_long(boundedSize),
			MTP_vector<MTPPhotoSize>(std::move(thumbs)),
			MTPVector<MTPVideoSize>(),
			MTP_int(file.dcId),
			MTP_vector<MTPDocumentAttribute>(std::move(attributes))),
		MTPVector<MTPDocument>(), // alt_documents
		MTPPhoto(), // video_cover
		MTPint(), // video_timestamp
		ttl ? MTP_int(ttl) : MTPint());
}

[[nodiscard]] MTPMessageMedia FileMedia(
		const HistoryRecord &record,
		const EncryptedFileInfo &file,
		const SecretTLDecryptedMessageMedia &media) {
	const auto ttl = MediaRequiresContentRead(media, record.ttl)
		? record.ttl
		: 0;
	const auto voice = [](int duration) {
		return QVector<MTPDocumentAttribute>{
			Audio(true, duration, nullptr, nullptr, nullptr),
		};
	};
	return media.match([&](const SecretTLDdecryptedMessageMediaPhoto8 &d) {
		return Photo(
			record,
			file,
			ttl,
			Thumb(d.vthumb(), d.vthumb_w(), d.vthumb_h()),
			d.vw(),
			d.vh(),
			MTP_int(int(std::min<int64>(
				file.size,
				std::numeric_limits<int>::max()))));
	}, [&](const SecretTLDdecryptedMessageMediaPhoto &d) {
		return Photo(
			record,
			file,
			ttl,
			Thumb(d.vthumb(), d.vthumb_w(), d.vthumb_h()),
			d.vw(),
			d.vh(),
			MTP_int(int(std::min<int64>(
				file.size,
				std::numeric_limits<int>::max()))));
	}, [&](const SecretTLDdecryptedMessageMediaVideo8 &d) {
		return Document(
			record,
			file,
			ttl,
			Thumb(d.vthumb(), d.vthumb_w(), d.vthumb_h()),
			MTP_string("video/mp4"),
			d.vsize().v,
			{ Video(false, d.vduration().v, d.vw(), d.vh()) });
	}, [&](const SecretTLDdecryptedMessageMediaVideo23 &d) {
		return Document(
			record,
			file,
			ttl,
			Thumb(d.vthumb(), d.vthumb_w(), d.vthumb_h()),
			d.vmime_type(),
			d.vsize().v,
			{ Video(false, d.vduration().v, d.vw(), d.vh()) });
	}, [&](const SecretTLDdecryptedMessageMediaVideo &d) {
		return Document(
			record,
			file,
			ttl,
			Thumb(d.vthumb(), d.vthumb_w(), d.vthumb_h()),
			d.vmime_type(),
			d.vsize().v,
			{ Video(false, d.vduration().v, d.vw(), d.vh()) });
	}, [&](const SecretTLDdecryptedMessageMediaDocument8 &d) {
		return Document(
			record,
			file,
			ttl,
			Thumb(d.vthumb(), d.vthumb_w(), d.vthumb_h()),
			d.vmime_type(),
			d.vsize().v,
			{ MTP_documentAttributeFilename(d.vfile_name()) });
	}, [&](const SecretTLDdecryptedMessageMediaDocument46 &d) {
		return Document(
			record,
			file,
			ttl,
			Thumb(d.vthumb(), d.vthumb_w(), d.vthumb_h()),
			d.vmime_type(),
			d.vsize().v,
			Attributes(d.vattributes().v));
	}, [&](const SecretTLDdecryptedMessageMediaDocument &d) {
		return Document(
			record,
			file,
			ttl,
			Thumb(d.vthumb(), d.vthumb_w(), d.vthumb_h()),
			d.vmime_type(),
			d.vsize().v,
			Attributes(d.vattributes().v));
	}, [&](const SecretTLDdecryptedMessageMediaAudio8 &d) {
		return Document(
			record,
			file,
			ttl,
			{},
			MTP_string("audio/ogg"),
			d.vsize().v,
			voice(d.vduration().v));
	}, [&](const SecretTLDdecryptedMessageMediaAudio &d) {
		return Document(
			record,
			file,
			ttl,
			{},
			d.vmime_type(),
			d.vsize().v,
			voice(d.vduration().v));
	}, [](const auto &) {
		return MTPMessageMedia(MTP_messageMediaEmpty());
	});
}

[[nodiscard]] QString StickerSetShortName(
		not_null<DocumentData*> document,
		const StickerSetIdentifier &set) {
	if (!set.shortName.isEmpty() || !set.id) {
		return set.shortName;
	}
	const auto &sets = document->owner().stickers().sets();
	const auto i = sets.find(set.id);
	return (i != end(sets)) ? i->second->shortName : QString();
}

[[nodiscard]] MTPGeoPoint GeoPoint(
		const MTPdouble &lat,
		const MTPdouble &lon) {
	return MTP_geoPoint(
		MTP_flags(MTPDgeoPoint::Flag(0)),
		lon,
		lat,
		MTP_long(0), // access_hash
		MTPint()); // accuracy_radius
}

[[nodiscard]] PreparedThumb MakePreparedThumb(
		const FilePrepareResult &file) {
	auto source = file.thumb;
	if (source.isNull() && !file.photoThumbs.empty()) {
		source = begin(file.photoThumbs)->second.image;
	}
	if (source.isNull()) {
		return {};
	}
	const auto image = (source.width() > kThumbSide
		|| source.height() > kThumbSide)
		? source.scaled(
			kThumbSide,
			kThumbSide,
			Qt::KeepAspectRatio,
			Qt::SmoothTransformation)
		: source;
	auto result = PreparedThumb{ .w = image.width(), .h = image.height() };
	auto buffer = QBuffer(&result.bytes);
	if (!image.save(&buffer, "JPG", kThumbQuality)) {
		return {};
	}
	return result;
}

[[nodiscard]] QVector<SecretTLDocumentAttribute> SecretAttributes(
		const QVector<MTPDocumentAttribute> &list) {
	using AudioFlag = SecretTLDdocumentAttributeAudio::Flag;
	using VideoFlag = SecretTLDdocumentAttributeVideo::Flag;
	auto result = QVector<SecretTLDocumentAttribute>();
	for (const auto &attribute : list) {
		attribute.match([&](const MTPDdocumentAttributeImageSize &d) {
			result.push_back(
				secret_documentAttributeImageSize(d.vw(), d.vh()));
		}, [&](const MTPDdocumentAttributeAnimated &) {
			result.push_back(secret_documentAttributeAnimated());
		}, [&](const MTPDdocumentAttributeVideo &d) {
			result.push_back(secret_documentAttributeVideo(
				secret_flags(d.is_round_message()
					? VideoFlag::f_round_message
					: VideoFlag(0)),
				secret_int(int(d.vduration().v)),
				d.vw(),
				d.vh()));
		}, [&](const MTPDdocumentAttributeAudio &d) {
			const auto title = d.vtitle();
			const auto performer = d.vperformer();
			const auto waveform = d.vwaveform();
			result.push_back(secret_documentAttributeAudio(
				secret_flags(AudioFlag(0)
					| (d.is_voice() ? AudioFlag::f_voice : AudioFlag(0))
					| (title ? AudioFlag::f_title : AudioFlag(0))
					| (performer ? AudioFlag::f_performer : AudioFlag(0))
					| (waveform ? AudioFlag::f_waveform : AudioFlag(0))),
				d.vduration(),
				title ? *title : SecretTLstring(),
				performer ? *performer : SecretTLstring(),
				waveform ? *waveform : SecretTLbytes()));
		}, [&](const MTPDdocumentAttributeFilename &d) {
			result.push_back(
				secret_documentAttributeFilename(d.vfile_name()));
		}, [](const auto &) {
		});
	}
	return result;
}

[[nodiscard]] QSize LargestPhotoSize(const MTPPhoto &photo) {
	auto result = QSize(0, 0);
	photo.match([&](const MTPDphoto &data) {
		for (const auto &entry : data.vsizes().v) {
			entry.match([&](const MTPDphotoSize &d) {
				if (d.vw().v * d.vh().v > result.width() * result.height()) {
					result = QSize(d.vw().v, d.vh().v);
				}
			}, [](const auto &) {
			});
		}
	}, [](const MTPDphotoEmpty &) {
	});
	return result;
}

} // namespace

bool IsStoredMedia(const SecretTLDecryptedMessageMedia &media) {
	switch (media.type()) {
	case secretc_decryptedMessageMediaExternalDocument:
	case secretc_decryptedMessageMediaGeoPoint:
	case secretc_decryptedMessageMediaVenue:
	case secretc_decryptedMessageMediaContact:
		return true;
	}
	return FileFromMedia(media).has_value();
}

std::optional<MediaFile> FileFromMedia(
		const SecretTLDecryptedMessageMedia &media) {
	return media.match([](const SecretTLDdecryptedMessageMediaPhoto8 &d) {
		return std::make_optional(FileOf(d));
	}, [](const SecretTLDdecryptedMessageMediaPhoto &d) {
		return std::make_optional(FileOf(d));
	}, [](const SecretTLDdecryptedMessageMediaVideo8 &d) {
		return std::make_optional(FileOf(d));
	}, [](const SecretTLDdecryptedMessageMediaVideo23 &d) {
		return std::make_optional(FileOf(d));
	}, [](const SecretTLDdecryptedMessageMediaVideo &d) {
		return std::make_optional(FileOf(d));
	}, [](const SecretTLDdecryptedMessageMediaDocument8 &d) {
		return std::make_optional(FileOf(d));
	}, [](const SecretTLDdecryptedMessageMediaDocument46 &d) {
		return std::make_optional(FileOf(d));
	}, [](const SecretTLDdecryptedMessageMediaDocument &d) {
		return std::make_optional(FileOf(d));
	}, [](const SecretTLDdecryptedMessageMediaAudio8 &d) {
		return std::make_optional(FileOf(d));
	}, [](const SecretTLDdecryptedMessageMediaAudio &d) {
		return std::make_optional(FileOf(d));
	}, [](const auto &) {
		return std::optional<MediaFile>();
	});
}

QString CaptionFromMedia(const SecretTLDecryptedMessageMedia &media) {
	return media.match([](const SecretTLDdecryptedMessageMediaPhoto &d) {
		return qs(d.vcaption());
	}, [](const SecretTLDdecryptedMessageMediaVideo &d) {
		return qs(d.vcaption());
	}, [](const SecretTLDdecryptedMessageMediaDocument46 &d) {
		return qs(d.vcaption());
	}, [](const SecretTLDdecryptedMessageMediaDocument &d) {
		return qs(d.vcaption());
	}, [](const auto &) {
		return QString();
	});
}

int32 AdjustedMediaTtl(
		const SecretTLDecryptedMessageMedia &media,
		int32 ttl) {
	auto result = std::clamp(ttl, int32(0), kMaxMessageTtl);
	if (!result) {
		return 0;
	}
	const auto duration = MediaDuration(media);
	if (duration > 0) {
		result = int32(std::min<int64>(
			kMaxMessageTtl,
			std::max<int64>(result, int64(duration) + 1)));
	}
	return result;
}

bool MediaRequiresContentRead(
		const SecretTLDecryptedMessageMedia &media,
		int32 ttl) {
	return ttl > 0
		&& ttl <= kContentReadTtlLimit
		&& IsTimedMedia(media);
}

bool MediaFileSizeValid(
		const SecretTLDecryptedMessageMedia &media,
		const std::optional<EncryptedFileInfo> &file) {
	const auto parsed = FileFromMedia(media);
	if (!parsed) {
		return true;
	} else if (!file
		|| parsed->size <= 0
		|| file->size < parsed->size) {
		return false;
	}
	switch (media.type()) {
	case secretc_decryptedMessageMediaPhoto8:
	case secretc_decryptedMessageMediaPhoto:
		return file->size <= Storage::kMaxFileInMemory;
	}
	return true;
}

bool IsSecretFile(not_null<Main::Session*> session, uint64 id) {
	return session->ayuSecret().file(id) != nullptr;
}

std::optional<QByteArray> DecryptSecretFile(
		not_null<Main::Session*> session,
		uint64 id,
		const QByteArray &encrypted) {
	const auto file = session->ayuSecret().file(id);
	if (!file) {
		return std::nullopt;
	}
	auto decrypted = Crypto::DecryptFile(
		file->key,
		bytes::make_span(encrypted),
		file->size);
	if (!decrypted) {
		return std::nullopt;
	} else if (decrypted->size()
		> size_t(std::numeric_limits<qsizetype>::max())) {
		Crypto::ZeroAndClear(*decrypted);
		return std::nullopt;
	}
	auto result = QByteArray(
		reinterpret_cast<const char*>(decrypted->data()),
		qsizetype(decrypted->size()));
	Crypto::ZeroAndClear(*decrypted);
	return result;
}

QByteArray SerializeMedia(const SecretTLDecryptedMessageMedia &media) {
	auto buffer = mtpBuffer();
	media.write(buffer);
	auto result = QByteArray(
		reinterpret_cast<const char*>(buffer.constData()),
		int(buffer.size() * sizeof(mtpPrime)));
	bytes::set_random(bytes::make_span(buffer.data(), buffer.size()));
	return result;
}

std::optional<SecretTLDecryptedMessageMedia> ParseMedia(
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
	auto result = SecretTLDecryptedMessageMedia();
	if (!result.read(from, end) || from != end) {
		return std::nullopt;
	}
	return result;
}

std::optional<SecretTLDecryptedMessageMedia> StickerMedia(
		not_null<DocumentData*> document) {
	const auto sticker = document->sticker();
	auto accessHash = uint64(0);
	document->mtpInput().match([&](const MTPDinputDocument &data) {
		accessHash = data.vaccess_hash().v;
	}, [](const MTPDinputDocumentEmpty &) {
	});
	if (!sticker || !accessHash || !document->getDC()) {
		return std::nullopt;
	}
	const auto shortName = StickerSetShortName(document, sticker->set);
	auto attributes = QVector<SecretTLDocumentAttribute>();
	if (!document->dimensions.isEmpty()) {
		attributes.push_back(secret_documentAttributeImageSize(
			secret_int(document->dimensions.width()),
			secret_int(document->dimensions.height())));
	}
	attributes.push_back(secret_documentAttributeSticker(
		secret_string(sticker->alt),
		(shortName.isEmpty()
			? secret_inputStickerSetEmpty()
			: secret_inputStickerSetShortName(secret_string(shortName)))));
	return secret_decryptedMessageMediaExternalDocument(
		secret_long(document->id),
		secret_long(accessHash),
		secret_int(document->date),
		secret_string(document->mimeString()),
		secret_int(int(document->size)),
		secret_photoSizeEmpty(secret_string()),
		secret_int(document->getDC()),
		secret_vector<SecretTLDocumentAttribute>(std::move(attributes)));
}

std::optional<SecretTLDecryptedMessageMedia> PreparedMedia(
		const FilePrepareResult &file,
		const Crypto::FileKey &key,
		int64 size,
		int layer) {
	const auto thumb = MakePreparedThumb(file);
	if (file.type == SendMediaType::Photo) {
		const auto dimensions = LargestPhotoSize(file.photo);
		if (dimensions.isEmpty()) {
			return std::nullopt;
		}
		return secret_decryptedMessageMediaPhoto(
			secret_bytes(thumb.bytes),
			secret_int(thumb.w),
			secret_int(thumb.h),
			secret_int(dimensions.width()),
			secret_int(dimensions.height()),
			secret_int(int(size)),
			secret_bytes(key.key),
			secret_bytes(key.iv),
			secret_string());
	}
	auto attributes = file.document.match([](const MTPDdocument &data) {
		return SecretAttributes(data.vattributes().v);
	}, [](const MTPDdocumentEmpty &) {
		return QVector<SecretTLDocumentAttribute>();
	});
	if (layer >= kLongSizeLayer) {
		return secret_decryptedMessageMediaDocument(
			secret_bytes(thumb.bytes),
			secret_int(thumb.w),
			secret_int(thumb.h),
			secret_string(file.filemime),
			secret_long(uint64(size)),
			secret_bytes(key.key),
			secret_bytes(key.iv),
			secret_vector<SecretTLDocumentAttribute>(std::move(attributes)),
			secret_string());
	} else if (size > std::numeric_limits<int32>::max()) {
		return std::nullopt;
	}
	return secret_decryptedMessageMediaDocument46(
		secret_bytes(thumb.bytes),
		secret_int(thumb.w),
		secret_int(thumb.h),
		secret_string(file.filemime),
		secret_int(int(size)),
		secret_bytes(key.key),
		secret_bytes(key.iv),
		secret_vector<SecretTLDocumentAttribute>(std::move(attributes)),
		secret_string());
}

MTPMessageMedia MediaFromRecord(
		not_null<Main::Session*> session,
		const HistoryRecord &record) {
	const auto media = record.media.isEmpty()
		? std::nullopt
		: ParseMedia(record.media);
	if (!media) {
		return MTP_messageMediaEmpty();
	} else if (FileFromMedia(*media)) {
		return record.file
			? FileMedia(record, *record.file, *media)
			: MTPMessageMedia(MTP_messageMediaEmpty());
	}
	return media->match([&](
			const SecretTLDdecryptedMessageMediaExternalDocument &data) {
		return ExternalDocument(session, data);
	}, [](const SecretTLDdecryptedMessageMediaGeoPoint &data) {
		return MTPMessageMedia(
			MTP_messageMediaGeo(GeoPoint(data.vlat(), data.vlong())));
	}, [](const SecretTLDdecryptedMessageMediaVenue &data) {
		return MTPMessageMedia(MTP_messageMediaVenue(
			GeoPoint(data.vlat(), data.vlong()),
			data.vtitle(),
			data.vaddress(),
			data.vprovider(),
			data.vvenue_id(),
			MTP_string())); // venue_type
	}, [](const SecretTLDdecryptedMessageMediaContact &data) {
		return MTPMessageMedia(MTP_messageMediaContact(
			data.vphone_number(),
			data.vfirst_name(),
			data.vlast_name(),
			MTP_string(), // vcard
			MTP_long(data.vuser_id().v)));
	}, [](const auto &) {
		return MTPMessageMedia(MTP_messageMediaEmpty());
	});
}

} // namespace AyuSecret
