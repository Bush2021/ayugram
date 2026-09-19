// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/secret/ui/secret_chat_menu.h"

#include "ayu/secret/data_secret_chat.h"
#include "ayu/secret/secret_chats.h"
#include "ayu/secret/ui/secret_key_box.h"
#include "base/weak_qptr.h"
#include "boxes/peer_list_box.h"
#include "boxes/peer_list_controllers.h"
#include "data/data_user.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "ui/boxes/confirm_box.h"
#include "ui/layers/generic_box.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"

namespace AyuSecret {
namespace {

class UserController final : public ContactsBoxController {
public:
	UserController(
		not_null<Main::Session*> session,
		Fn<void(not_null<UserData*>)> chosen);

protected:
	std::unique_ptr<PeerListRow> createRow(
		not_null<UserData*> user) override;
	void rowClicked(not_null<PeerListRow*> row) override;

private:
	const Fn<void(not_null<UserData*>)> _chosen;

};

UserController::UserController(
	not_null<Main::Session*> session,
	Fn<void(not_null<UserData*>)> chosen)
: ContactsBoxController(session)
, _chosen(std::move(chosen)) {
}

std::unique_ptr<PeerListRow> UserController::createRow(
		not_null<UserData*> user) {
	return CanStartSecretChat(user)
		? ContactsBoxController::createRow(user)
		: nullptr;
}

void UserController::rowClicked(not_null<PeerListRow*> row) {
	if (const auto user = row->peer()->asUser()) {
		_chosen(user);
	}
}

} // namespace

bool CanStartSecretChat(not_null<UserData*> user) {
	return !user->isSelf()
		&& !user->isBot()
		&& !user->isInaccessible()
		&& !user->isServiceUser()
		&& !user->isRepliesChat()
		&& !user->isVerifyCodes();
}

void StartSecretChat(
		not_null<Window::SessionController*> controller,
		not_null<UserData*> user) {
	const auto weak = base::make_weak(controller);
	const auto opened = [=](SecretChatData *peer) {
		const auto strong = weak.get();
		if (!strong) {
			return;
		} else if (!peer) {
			strong->showToast(tr::ayu_SecretChatStartFailed(tr::now));
			return;
		}
		strong->showPeerHistory(peer, Window::SectionShow::Way::Forward);
	};
	controller->show(Ui::MakeConfirmBox({
		.text = tr::ayu_SecretChatStartConfirm(),
		.confirmed = [=](Fn<void()> &&close) {
			user->session().ayuSecret().createWith(user, opened);
			close();
		},
		.confirmText = tr::ayu_SecretChatStart(),
	}));
}

void ShowNewSecretChatBox(
		not_null<Window::SessionController*> controller) {
	const auto box = std::make_shared<base::weak_qptr<PeerListBox>>();
	auto list = std::make_unique<UserController>(
		&controller->session(),
		[=](not_null<UserData*> user) {
			if (const auto strong = box->get()) {
				strong->closeBox();
			}
			StartSecretChat(controller, user);
		});
	auto init = [=](not_null<PeerListBox*> peerBox) {
		*box = peerBox;
		peerBox->setTitle(tr::ayu_NewSecretChat());
		peerBox->addButton(tr::lng_cancel(), [=] {
			peerBox->closeBox();
		});
	};
	controller->show(
		Box<PeerListBox>(std::move(list), std::move(init)));
}

namespace {

void ReportSecretChat(
		not_null<Window::SessionController*> controller,
		not_null<SecretChatData*> peer) {
	const auto weak = base::make_weak(controller);
	controller->show(Ui::MakeConfirmBox({
		.text = tr::ayu_SecretChatReportSure(
			tr::now,
			lt_user,
			peer->user()->shortName()),
		.confirmed = [=](Fn<void()> &&close) {
			peer->session().ayuSecret().reportSpam(peer);
			close();
			if (const auto strong = weak.get()) {
				strong->showToast(tr::lng_report_spam_done(tr::now));
			}
		},
		.confirmText = tr::lng_report_spam_ok(),
		.confirmStyle = &st::attentionBoxButton,
	}));
}

} // namespace

void AddSecretChatActions(
		PeerData *peer,
		not_null<Window::SessionController*> controller,
		const Window::PeerMenuCallback &addCallback) {
	if (const auto secret = peer ? peer->asSecretChat() : nullptr) {
		if (secret->isReady()) {
			addCallback(tr::ayu_SecretChatKey(tr::now), [=] {
				controller->show(Box(SecretKeyBox, secret));
			}, &st::menuIconLock);
		}
		if (!secret->creator() && secret->user()) {
			addCallback(tr::lng_report_spam(tr::now), [=] {
				ReportSecretChat(controller, secret);
			}, &st::menuIconReport);
		}
		return;
	}
	const auto user = peer ? peer->asUser() : nullptr;
	if (!user || !CanStartSecretChat(user)) {
		return;
	}
	addCallback(tr::ayu_SecretChatStart(tr::now), [=] {
		StartSecretChat(controller, user);
	}, &st::menuIconLock);
}

} // namespace AyuSecret
