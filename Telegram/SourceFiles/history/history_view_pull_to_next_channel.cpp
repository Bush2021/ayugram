/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/history_view_pull_to_next_channel.h"

#include "base/event_filter.h"
#include "base/platform/base_platform_haptic.h"
#include "data/data_chat_filters.h"
#include "data/data_peer.h"
#include "data/data_session.h"
#include "data/components/sponsored_messages.h"
#include "dialogs/dialogs_indexed_list.h"
#include "dialogs/dialogs_main_list.h"
#include "dialogs/dialogs_row.h"
#include "history/history.h"
#include "main/main_session.h"
#include "ui/chat/continuous_scroll.h"
#include "ui/widgets/elastic_scroll.h"
#include "ui/ui_utility.h"
#include "window/window_session_controller.h"
#include "styles/style_chat_helpers.h"

#include <QtGui/QWheelEvent>

namespace HistoryView {
namespace {

constexpr auto kDirectionLock = 8.;
constexpr auto kResetReachedOn = 0.95;

[[nodiscard]] History *FindNextUnreadChannel(
		not_null<Window::SessionController*> controller,
		not_null<PeerData*> current) {
	auto &data = controller->session().data();
	const auto filterId = controller->activeChatsFilterCurrent();
	const auto list = filterId
		? data.chatsFilters().chatsList(filterId)
		: data.chatsList();
	for (const auto &row : list->indexed()->all()) {
		const auto history = row->history();
		if (!history) {
			continue;
		}
		const auto peer = history->peer;
		if (peer != current
			&& peer->isBroadcast()
			&& history->unreadCount() > 0) {
			return history;
		}
	}
	return nullptr;
}

} // namespace

PullToNextChannel::PullToNextChannel(
	not_null<Ui::RpWidget*> parent,
	not_null<Ui::ContinuousScroll*> scroll,
	not_null<Window::SessionController*> controller)
: _parent(parent)
, _scroll(scroll)
, _controller(controller) {
}

PullToNextChannel::~PullToNextChannel() = default;

void PullToNextChannel::attachToContent(not_null<Ui::RpWidget*> inner) {
	reset();
	_filter = base::unique_qptr<QObject>(base::install_event_filter(
		inner,
		[=](not_null<QEvent*> e) {
			return (e->type() == QEvent::Wheel
					&& processWheel(static_cast<QWheelEvent*>(e.get())))
				? base::EventFilterResult::Cancel
				: base::EventFilterResult::Continue;
		}));
}

void PullToNextChannel::setHistory(History *history) {
	if (_history == history) {
		return;
	}
	_history = history;
	reset();
}

bool PullToNextChannel::active() const {
	return _history
		&& _history->peer->isBroadcast()
		&& atBottom()
		&& !_controller->session().sponsoredMessages().hasUnshownFor(_history);
}

bool PullToNextChannel::atBottom() const {
	return (_scroll->scrollTop() >= _scroll->scrollTopMax())
		&& _history->loadedAtBottom();
}

bool PullToNextChannel::processWheel(not_null<QWheelEvent*> e) {
	const auto phase = e->phase();
	if (phase == Qt::NoScrollPhase) {
		return false;
	} else if (phase == Qt::ScrollBegin) {
		reset();
		return false;
	} else if (phase == Qt::ScrollEnd || phase == Qt::ScrollMomentum) {
		return release();
	} else if (!_engaged
		&& (_gaveUp
			|| !_history
			|| !_history->peer->isBroadcast()
			|| !atBottom())) {
		return false;
	}
	const auto delta = Ui::ScrollDeltaF(e);
	return applyDelta(delta.x(), delta.y());
}

bool PullToNextChannel::applyDelta(float64 deltaX, float64 deltaY) {
	if (!_engaged) {
		_swipeX += deltaX;
		_swipeY += deltaY;
		const auto down = -_swipeY;
		const auto sideways = std::abs(_swipeX);
		if (sideways > kDirectionLock && sideways >= down) {
			_gaveUp = true;
			return false;
		} else if (down <= kDirectionLock || down <= sideways || !active()) {
			return false;
		}
		_engaged = true;
		_accumulated = down;
		_next = FindNextUnreadChannel(_controller, _history->peer);
	} else {
		_accumulated = std::max(0., _accumulated - deltaY);
	}
	_offset = Ui::OverscrollFromAccumulated(
		int(base::SafeRound(_accumulated)));
	if (_offset <= 0.) {
		reset();
		return true;
	}
	const auto threshold = float64(st::historyPullNextThreshold);
	const auto ratio = threshold ? (_offset / threshold) : 0.;
	if (_next && !_reached && ratio >= 1.) {
		_reached = true;
		base::Platform::Haptic();
	} else if (_reached && ratio < kResetReachedOn) {
		_reached = false;
	}
	return true;
}

bool PullToNextChannel::release() {
	if (!_engaged) {
		return false;
	}
	const auto next = _next;
	const auto ready = (_offset >= float64(st::historyPullNextThreshold))
		&& next
		&& next->unreadCount() > 0;
	reset();
	if (ready) {
		jumpTo(next);
	}
	return true;
}

void PullToNextChannel::reset() {
	_accumulated = 0.;
	_offset = 0.;
	_swipeX = 0.;
	_swipeY = 0.;
	_engaged = false;
	_reached = false;
	_gaveUp = false;
	_next = nullptr;
}

void PullToNextChannel::updateGeometry() {
}

void PullToNextChannel::jumpTo(not_null<History*> history) {
	_controller->showPeerHistory(
		history,
		Window::SectionShow::Way::ClearStack);
}

} // namespace HistoryView
