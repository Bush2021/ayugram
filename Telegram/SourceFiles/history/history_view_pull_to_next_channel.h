/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/unique_qptr.h"

class History;
class QEvent;
class QWheelEvent;

namespace Ui {
class RpWidget;
class ContinuousScroll;
} // namespace Ui

namespace Window {
class SessionController;
} // namespace Window

namespace HistoryView {

class PullToNextChannel final {
public:
	PullToNextChannel(
		not_null<Ui::RpWidget*> parent,
		not_null<Ui::ContinuousScroll*> scroll,
		not_null<Window::SessionController*> controller);
	~PullToNextChannel();

	void attachToContent(not_null<Ui::RpWidget*> inner);

	void setHistory(History *history);

	void updateGeometry();

private:
	[[nodiscard]] bool active() const;
	[[nodiscard]] bool atBottom() const;
	[[nodiscard]] bool processWheel(not_null<QWheelEvent*> e);
	[[nodiscard]] bool applyDelta(float64 deltaX, float64 deltaY);
	[[nodiscard]] bool release();
	void reset();
	void jumpTo(not_null<History*> history);

	const not_null<Ui::RpWidget*> _parent;
	const not_null<Ui::ContinuousScroll*> _scroll;
	const not_null<Window::SessionController*> _controller;

	History *_history = nullptr;
	History *_next = nullptr;

	base::unique_qptr<QObject> _filter;

	float64 _accumulated = 0.;
	float64 _offset = 0.;
	float64 _swipeX = 0.;
	float64 _swipeY = 0.;
	bool _engaged = false;
	bool _reached = false;
	bool _gaveUp = false;

};

} // namespace HistoryView
