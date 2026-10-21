// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#include "ayu/features/message_selection/select_between.h"

#include "config.h"
#include "crl/crl_on_main.h"
#include "data/data_groups.h"
#include "data/data_session.h"
#include "history/history_item.h"
#include "main/main_session.h"

namespace AyuFeatures {

SelectBetween::SelectBetween(
		not_null<Main::Session*> session,
		Source source,
		Less less,
		Selectable selectable,
		Fn<void()> changed)
: _session(session)
, _source(std::move(source))
, _less(std::move(less))
, _selectable(std::move(selectable))
, _changed(std::move(changed)) {
}

MessageIdsList SelectBetween::group(FullMsgId id) const {
	auto result = MessageIdsList();
	const auto item = _session->data().message(id);
	const auto album = item ? _session->data().groups().find(item) : nullptr;
	if (album) {
		for (const auto &part : album->items) {
			result.push_back(part->fullId());
		}
	} else {
		result.push_back(id);
	}
	return result;
}

bool SelectBetween::update(HistoryItemsList selected) {
	ranges::sort(selected, _less);
	auto ids = MessageIdsList();
	for (const auto &item : selected) {
		ids.push_back(item->fullId());
	}
	if (_selected != ids) {
		_lifetime.destroy();
		_done = nullptr;
		_selected = std::move(ids);
		_state = State::Loading;
		_ids.clear();
		_hasRange = false;
		_extendScheduled = false;
		if (_selected.size() >= 2) {
			const auto firstGroup = group(_selected.front());
			const auto lastGroup = group(_selected.back());
			if (firstGroup.front() != lastGroup.front()) {
				_first = selected.front()->position();
				_last = _selected.back();
				_hasRange = true;
				_limit = MaxSelectedItems;
				subscribe();
			}
		}
	}
	return _hasRange && possible();
}

bool SelectBetween::possible() const {
	return (_state != State::Ready) || !_ids.empty();
}

void SelectBetween::subscribe() {
	_lifetime.destroy();
	_source(
		_first,
		0,
		_limit
	) | rpl::on_next([=](const Data::MessagesSlice &slice) {
		consume(slice);
	}, _lifetime);
}

void SelectBetween::collect(const MessageIdsList &loaded) {
	_state = State::Loading;
	_ids.clear();
	const auto from = ranges::find(loaded, _first.fullId);
	const auto selectable = [=](FullMsgId id) {
		const auto item = _session->data().message(id);
		return item && _selectable(item);
	};
	for (auto i = from; i != end(loaded); ++i) {
		const auto items = group(*i);
		if (ranges::all_of(items, selectable)) {
			for (const auto &id : items) {
				if (!ranges::contains(_selected, id)
					&& !ranges::contains(_ids, id)) {
					_ids.push_back(id);
				}
			}
			if (int(_selected.size() + _ids.size()) > MaxSelectedItems) {
				_state = State::TooMany;
				_ids.clear();
				return;
			}
		}
		if (*i == _last) {
			_state = State::Ready;
			return;
		}
	}
	_ids.clear();
}

void SelectBetween::consume(const Data::MessagesSlice &slice) {
	const auto wasPossible = possible();
	collect(slice.ids);
	if (_state == State::Loading
		&& ranges::contains(slice.ids, _first.fullId)) {
		if (slice.skippedAfter == 0) {
			_state = State::Ready;
		} else if (int(slice.ids.size()) > _limit && !_extendScheduled) {
			_extendScheduled = true;
			const auto selected = _selected;
			crl::on_main(this, [=] {
				if (_selected != selected) {
					return;
				}
				_extendScheduled = false;
				if (_state == State::Loading) {
					_limit += MaxSelectedItems;
					subscribe();
				}
			});
		}
	}
	if (possible() != wasPossible || _done) {
		notify();
	}
}

void SelectBetween::select(Done done) {
	if (!_hasRange) {
		return;
	}
	_done = std::move(done);
	notify();
}

void SelectBetween::notify() {
	if (_notifyScheduled) {
		return;
	}
	_notifyScheduled = true;
	crl::on_main(this, [=] {
		_notifyScheduled = false;
		_changed();
		if (_done && _state != State::Loading) {
			auto done = base::take(_done);
			auto result = (_state == State::TooMany)
				? std::nullopt
				: std::make_optional(_ids);
			done(std::move(result));
		}
	});
}

} // namespace AyuFeatures
