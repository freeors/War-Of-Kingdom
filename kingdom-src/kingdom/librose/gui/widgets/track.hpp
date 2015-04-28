/* $Id: button.hpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
/*
   Copyright (C) 2008 - 2012 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#ifndef GUI_WIDGETS_TRACK_HPP_INCLUDED
#define GUI_WIDGETS_TRACK_HPP_INCLUDED

#include "gui/widgets/control.hpp"
#include "gui/widgets/clickable.hpp"

namespace gui2 {

/**
 * Simple push button.
 */
class ttrack: public tcontrol
{
public:
	enum tstate { ENABLED, DISABLED, COUNT };
	enum tstate2 { NORMAL, FOCUSSED, PRESSED };

	ttrack();
	~ttrack();

	/***** ***** ***** ***** Inherited ***** ***** ***** *****/

	/** Inherited from tcontrol. */
	void set_active(const bool active);
		

	/** Inherited from tcontrol. */
	bool get_active() const { return state_ != DISABLED; }

	/** Inherited from tcontrol. */
	unsigned get_state() const { return state_; }

	/** Inherited from tclickable. */
	void connect_click_handler(const event::tsignal_function& signal)
	{
		connect_signal_mouse_left_click(*this, signal);
	}

	/** Inherited from tclickable. */
	void disconnect_click_handler(const event::tsignal_function& signal)
	{
		disconnect_signal_mouse_left_click(*this, signal);
	}

	/***** ***** ***** setters / getters for members ***** ****** *****/
	void set_best_size(const tpoint& best_size) { best_size_ = best_size; }

	void set_callback_timer(boost::function<void (ttrack&, surface&, const tpoint&, int, bool)> callback)
		{ callback_timer_ = callback; }

	void broadcast_frame_buffer(surface& frame_buffer) { frame_buffer_ = frame_buffer; }

	surface& get_frame_buffer() { return frame_buffer_; }
	tpoint get_frame_offset() { return tpoint(last_x_offset_ + draw_offset_.x, last_y_offset_ + draw_offset_.y); }

	surface& background_surf() { return background_surf_; }
	tstate2 get_state2() const { return state2_; }

	void set_require_capture(bool val) { require_capture_ = val; }
	void set_timer_interval(int interval) { timer_interval_ = interval; }

private:
	/***** ***** ***** setters / getters for members ***** ****** *****/
	void timer_handler();

	/** Inherited from tcontrol. */
	tpoint calculate_best_size() const;

	/** Inherited from twidget. */
	void place(const tpoint& origin, const tpoint& size);

	/** Inherited from tcontrol. */
	void impl_draw_background(
			  surface& frame_buffer
			, int x_offset
			, int y_offset);

private:

	/**
	 * Possible states of the widget.
	 *
	 * Note the order of the states must be the same as defined in settings.hpp.
	 */
	void set_state(const tstate state);
	/**
	 * Current state of the widget.
	 *
	 * The state of the widget determines what to render and how the widget
	 * reacts to certain 'events'.
	 */
	tstate state_;

	void set_state2(const tstate2 state);

	tstate2 state2_;

	/** When we're used as a fixed size item, this holds the best size. */
	tpoint best_size_;

	unsigned long timer_;
	surface background_surf_;
	surface frame_buffer_;
	int last_x_offset_;
	int last_y_offset_;
	bool require_capture_;
	int timer_interval_;

	boost::function<void (ttrack&, surface&, const tpoint&, int, bool)> callback_timer_;

	/** Inherited from tcontrol. */
	const std::string& get_control_type() const;

	/***** ***** ***** signal handlers ***** ****** *****/

	void signal_handler_mouse_enter(const event::tevent event, bool& handled);

	void signal_handler_mouse_leave(const event::tevent event, bool& handled);

	void signal_handler_left_button_down(
			const event::tevent event, bool& handled);

	void signal_handler_left_button_up(
			const event::tevent event, bool& handled);
};

} // namespace gui2

#endif

