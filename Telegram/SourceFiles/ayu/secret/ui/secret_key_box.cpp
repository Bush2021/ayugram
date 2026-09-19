// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/ui/secret_key_box.h"

#include "ayu/secret/data_secret_chat.h"
#include "ayu/secret/secret_chats.h"
#include "ayu/secret/secret_crypto.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/layers/generic_box.h"
#include "ui/text/text_utilities.h"
#include "ui/widgets/labels.h"
#include "styles/style_ayu_styles.h"
#include "styles/style_boxes.h"
#include "styles/style_layers.h"

namespace AyuSecret {
namespace {

constexpr auto kPatternCells = 12;
constexpr auto kHexBytes = 32;

[[nodiscard]] QColor PatternColor(int index) {
	static const auto colors = std::array{
		QColor(0xff, 0xff, 0xff),
		QColor(0xd5, 0xe6, 0xf3),
		QColor(0x2d, 0x57, 0x75),
		QColor(0x2f, 0x99, 0xc9),
	};
	return colors[index];
}

[[nodiscard]] int PatternBits(bytes::const_span hash, int offset) {
	return (int(uchar(hash[offset / 8])) >> (offset % 8)) & 0x03;
}

[[nodiscard]] QString HexText(bytes::const_span hash) {
	auto result = QString();
	for (auto i = 0; i != kHexBytes; ++i) {
		if (i) {
			result.append((i % 8) ? ((i % 4) ? u" "_q : u"  "_q) : u"\n"_q);
		}
		result.append(u"%1"_q.arg(int(uchar(hash[i])), 2, 16, QChar('0')));
	}
	return result;
}

void AddPattern(not_null<Ui::GenericBox*> box, bytes::vector hash) {
	const auto size = st::ayuSecretKeyPatternSize;
	const auto row = box->addRow(
		object_ptr<Ui::RpWidget>(box),
		style::al_top);
	row->resize(size, size);
	row->paintRequest() | rpl::on_next([=] {
		auto p = QPainter(row);
		const auto cell = size / kPatternCells;
		const auto shift = (size - cell * kPatternCells) / 2;
		for (auto y = 0; y != kPatternCells; ++y) {
			for (auto x = 0; x != kPatternCells; ++x) {
				const auto offset = 2 * (y * kPatternCells + x);
				p.fillRect(
					shift + x * cell,
					shift + y * cell,
					cell,
					cell,
					PatternColor(PatternBits(hash, offset)));
			}
		}
	}, row->lifetime());
}

} // namespace

void SecretKeyBox(
		not_null<Ui::GenericBox*> box,
		not_null<SecretChatData*> peer) {
	box->setTitle(tr::ayu_SecretChatKey());
	box->setWidth(st::boxWideWidth);
	box->addButton(tr::lng_box_ok(), [=] { box->closeBox(); });

	const auto state = peer->session().ayuSecret().state(peer);
	auto hash = state
		? Crypto::KeyHashForVisualization(state->key)
		: bytes::vector();
	if (!hash.empty()) {
		box->addSkip(st::ayuSecretKeySkip);
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(Ui::Text::Wrapped(
				TextWithEntities{ HexText(hash) },
				EntityType::Code)),
			st::ayuSecretKeyHexLabel));
		box->addSkip(st::ayuSecretKeySkip);
		AddPattern(box, std::move(hash));
		box->addSkip(st::ayuSecretKeySkip);
	}
	const auto user = peer->secretChatUser();
	box->addRow(object_ptr<Ui::FlatLabel>(
		box,
		tr::ayu_SecretChatKeyDescription(
			lt_user,
			rpl::single(tr::bold(user ? user->name() : QString())),
			tr::marked),
		st::aboutLabel));
}

} // namespace AyuSecret
