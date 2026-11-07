// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/ui/boxes/common_messages_box.h"

#include "base/weak_ptr.h"
#include "boxes/peer_list_box.h"
#include "data/data_channel.h"
#include "data/data_session.h"
#include "data/data_user.h"
#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_helpers.h"
#include "history/view/history_view_item_preview.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "mtproto/sender.h"
#include "ui/text/format_values.h"
#include "window/window_session_controller.h"

namespace AyuUi {
namespace {

constexpr auto kChatsPerPage = 100;
constexpr auto kMessagesPerChat = 20;

class Row final : public PeerListRow {
public:
	Row(not_null<HistoryItem*> item, PeerListRowId id);

	[[nodiscard]] FullMsgId itemId() const {
		return _itemId;
	}
	[[nodiscard]] TimeId date() const {
		return _date;
	}

private:
	FullMsgId _itemId;
	TimeId _date = 0;

};

Row::Row(not_null<HistoryItem*> item, PeerListRowId id)
: PeerListRow(item->history()->peer, id)
, _itemId(item->fullId())
, _date(item->date()) {
	auto preview = item->toPreview({
		.hideSender = true,
		.generateImages = false,
	}).text.text;
	preview.replace(QChar('\n'), QChar(' '));
	setCustomStatus(Ui::FormatDateTime(ItemDateTime(item))
		+ u" · "_q
		+ preview);
}

class Controller final
	: public PeerListController
	, public base::has_weak_ptr {
public:
	Controller(
		not_null<UserData*> user,
		Fn<void(FullMsgId)> open);

	Main::Session &session() const override;
	void prepare() override;
	void rowClicked(not_null<PeerListRow*> row) override;

private:
	void requestChats();
	void searchNext();
	void processMessages(const MTPmessages_Messages &result);
	void finish();

	const not_null<UserData*> _user;
	const Fn<void(FullMsgId)> _open;
	MTP::Sender _api;
	std::vector<not_null<PeerData*>> _chats;
	PeerId _chatsOffset = 0;
	int _searched = 0;
	PeerListRowId _nextRowId = 0;

};

Controller::Controller(
	not_null<UserData*> user,
	Fn<void(FullMsgId)> open)
: _user(user)
, _open(std::move(open))
, _api(&user->session().mtp()) {
}

Main::Session &Controller::session() const {
	return _user->session();
}

void Controller::prepare() {
	setDescriptionText(tr::lng_contacts_loading(tr::now));
	requestChats();
}

void Controller::requestChats() {
	_api.request(MTPmessages_GetCommonChats(
		_user->inputUser(),
		MTP_long(peerIsChat(_chatsOffset)
			? peerToChat(_chatsOffset).bare
			: peerToChannel(_chatsOffset).bare),
		MTP_int(kChatsPerPage)
	)).done([=](const MTPmessages_Chats &result) {
		const auto &chats = result.match([](const auto &data) {
			return data.vchats().v;
		});
		const auto owner = &_user->owner();
		auto received = false;
		for (const auto &chat : chats) {
			if (const auto peer = owner->processChat(chat)) {
				received = true;
				_chatsOffset = peer->id;
				if (!peer->migrateTo() && !peer->isBroadcast()) {
					_chats.push_back(peer);
				}
			}
		}
		if (received) {
			requestChats();
		} else {
			searchNext();
		}
	}).fail([=] {
		searchNext();
	}).send();
}

void Controller::searchNext() {
	if (_searched >= int(_chats.size())) {
		finish();
		return;
	}
	const auto peer = _chats[_searched++];
	_api.request(MTPmessages_Search(
		MTP_flags(MTPmessages_Search::Flag::f_from_id),
		peer->input(),
		MTP_string(),
		_user->input(),
		MTP_inputPeerEmpty(), // saved_peer_id
		MTP_vector<MTPReaction>(), // saved_reaction
		MTP_int(0), // top_msg_id
		MTP_inputMessagesFilterEmpty(),
		MTP_int(0), // min_date
		MTP_int(0), // max_date
		MTP_int(0), // offset_id
		MTP_int(0), // add_offset
		MTP_int(kMessagesPerChat),
		MTP_int(0), // max_id
		MTP_int(0), // min_id
		MTP_long(0) // hash
	)).done([=](const MTPmessages_Messages &result) {
		processMessages(result);
		searchNext();
	}).fail([=] {
		searchNext();
	}).send();
}

void Controller::processMessages(const MTPmessages_Messages &result) {
	const auto owner = &_user->owner();
	const auto add = [&](const auto &data) {
		owner->processUsers(data.vusers());
		owner->processChats(data.vchats());
		for (const auto &message : data.vmessages().v) {
			if (!DateFromMessage(message)
				|| !owner->peerLoaded(PeerFromMessage(message))) {
				continue;
			}
			const auto item = owner->addNewMessage(
				message,
				MessageFlags(),
				NewMessageType::Existing);
			if (item) {
				delegate()->peerListAppendRow(
					std::make_unique<Row>(item, ++_nextRowId));
			}
		}
	};
	result.match([&](const MTPDmessages_messagesNotModified &) {
	}, [&](const auto &data) {
		add(data);
	});
	delegate()->peerListSortRows([](
			const PeerListRow &a,
			const PeerListRow &b) {
		return static_cast<const Row&>(a).date()
			> static_cast<const Row&>(b).date();
	});
	if (delegate()->peerListFullRowsCount() > 0) {
		setDescriptionText(QString());
	}
	delegate()->peerListRefreshRows();
}

void Controller::finish() {
	if (!delegate()->peerListFullRowsCount()) {
		setDescriptionText(tr::ayu_CommonMessagesEmpty(tr::now));
	}
}

void Controller::rowClicked(not_null<PeerListRow*> row) {
	_open(static_cast<Row*>(row.get())->itemId());
}

} // namespace

void ShowCommonMessagesBox(
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user) {
	const auto box = std::make_shared<base::weak_qptr<PeerListBox>>();
	const auto weak = base::make_weak(controller);
	auto list = std::make_unique<Controller>(user, [=](FullMsgId id) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		}
		const auto item = strong->session().data().message(id);
		if (!item) {
			return;
		}
		if (const auto opened = box->get()) {
			opened->closeBox();
		}
		strong->showMessage(item, Window::SectionShow::Way::Forward);
	});
	auto init = [=](not_null<PeerListBox*> peerBox) {
		*box = peerBox;
		peerBox->setTitle(tr::ayu_CommonMessages());
		peerBox->addButton(tr::lng_close(), [=] {
			peerBox->closeBox();
		});
	};
	controller->show(Box<PeerListBox>(std::move(list), std::move(init)));
}

} // namespace AyuUi
