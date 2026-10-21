// This is the source code of AyuGram for Desktop.
//
// We do not and cannot prevent the use of our code,
// but be respectful and credit the original author.
//
// Copyright @Bush2021, 2026
#pragma once

#include "base/weak_ptr.h"
#include "data/data_types.h"
#include "data/data_messages.h"
#include "rpl/lifetime.h"
#include "rpl/producer.h"

namespace Main {
class Session;
} // namespace Main

namespace AyuFeatures {

class SelectBetween final : public base::has_weak_ptr {
public:
	using Source = Fn<rpl::producer<Data::MessagesSlice>(
		Data::MessagePosition, int, int)>;
	using Less = Fn<bool(not_null<HistoryItem*>, not_null<HistoryItem*>)>;
	using Selectable = Fn<bool(not_null<HistoryItem*>)>;
	using Done = Fn<void(std::optional<MessageIdsList>)>;

	SelectBetween(
		not_null<Main::Session*> session,
		Source source,
		Less less,
		Selectable selectable,
		Fn<void()> changed);

	[[nodiscard]] bool update(HistoryItemsList selected);
	void select(Done done);

private:
	enum class State {
		Loading,
		Ready,
		TooMany,
	};

	void subscribe();
	void consume(const Data::MessagesSlice &slice);
	void collect(const MessageIdsList &loaded);
	void notify();
	[[nodiscard]] bool possible() const;
	[[nodiscard]] MessageIdsList group(FullMsgId id) const;

	const not_null<Main::Session*> _session;
	const Source _source;
	const Less _less;
	const Selectable _selectable;
	const Fn<void()> _changed;
	MessageIdsList _selected;
	Data::MessagePosition _first;
	FullMsgId _last;
	State _state = State::Loading;
	MessageIdsList _ids;
	Done _done;
	int _limit = 0;
	bool _hasRange = false;
	bool _notifyScheduled = false;
	bool _extendScheduled = false;
	rpl::lifetime _lifetime;

};

} // namespace AyuFeatures
