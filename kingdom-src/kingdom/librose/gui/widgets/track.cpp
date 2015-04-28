/* $Id: button.cpp 52533 2012-01-07 02:35:17Z shadowmaster $ */
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

#define GETTEXT_DOMAIN "rose-lib"

#include "gui/widgets/track.hpp"

#include "gui/auxiliary/log.hpp"
#include "gui/auxiliary/widget_definition/track.hpp"
#include "gui/auxiliary/window_builder/track.hpp"
#include "gui/widgets/settings.hpp"
#include "gui/widgets/window.hpp"
#include "gui/auxiliary/timer.hpp"

#include <boost/bind.hpp>

#define LOG_SCOPE_HEADER get_control_type() + " [" + id() + "] " + __func__
#define LOG_HEADER LOG_SCOPE_HEADER + ':'

namespace gui2 {

REGISTER_WIDGET(track)

ttrack::ttrack()
	: tcontrol(COUNT)
	, state_(ENABLED)
	, state2_(NORMAL)
	, best_size_(0, 0)
	, timer_(0)
	, last_x_offset_(0)
	, last_y_offset_(0)
	, require_capture_(true)
	, timer_interval_(500)
{
	connect_signal<event::MOUSE_ENTER>(boost::bind(
				&ttrack::signal_handler_mouse_enter, this, _2, _3));
	connect_signal<event::MOUSE_LEAVE>(boost::bind(
				&ttrack::signal_handler_mouse_leave, this, _2, _3));

	connect_signal<event::LEFT_BUTTON_DOWN>(boost::bind(
				&ttrack::signal_handler_left_button_down, this, _2, _3));
	connect_signal<event::LEFT_BUTTON_UP>(boost::bind(
				&ttrack::signal_handler_left_button_up, this, _2, _3));
}

ttrack::~ttrack()
{
	if (timer_) {
		remove_timer(timer_);
	}
}

void ttrack::set_active(const bool active)
{ 
	if (get_active() != active) {
		set_state(active ? ENABLED : DISABLED); 
	}
};

void ttrack::set_state(const tstate state)
{
	if (state != state_) {
		state_ = state;
		set_dirty(true);
	}
}

void ttrack::set_state2(const tstate2 state)
{
	if (state != state2_) {
		state2_ = state;
		// set_dirty(true);
	}
}

tpoint ttrack::calculate_best_size() const
{
	if (best_size_ != tpoint(0, 0)) {
		return best_size_;
	}
	return tcontrol::calculate_best_size();
}

void ttrack::place(const tpoint& origin, const tpoint& size)
{
	tcontrol::place(origin, size);
	if (!timer_) {
		timer_ = add_timer(timer_interval_, boost::bind(&ttrack::timer_handler, this), true);
		timer_handler();
	}
}

void ttrack::impl_draw_background(surface& frame_buffer, int x_offset, int y_offset)
{
	tcontrol::impl_draw_background(frame_buffer, x_offset, y_offset);

	SDL_Rect rc = get_rect();
	if (!background_surf_ || background_surf_->w != w_ || background_surf_->h != h_) {
		background_surf_.assign(get_surface_portion(frame_buffer, rc));
	} else {
		sdl_blit(frame_buffer, &rc, background_surf_, NULL);
	}

	frame_buffer_ = frame_buffer;
	last_x_offset_ = x_offset;
	last_y_offset_ = y_offset;
	if (callback_timer_) {
		callback_timer_(*this, frame_buffer, get_frame_offset(), state2_, false);
	}
}

void ttrack::timer_handler()
{
	if (callback_timer_) {
		if (frame_buffer_) {
			callback_timer_(*this, frame_buffer_, get_frame_offset(), state2_, true);
		}
	}	
}

const std::string& ttrack::get_control_type() const
{
	static const std::string type = "track";
	return type;
}

void ttrack::signal_handler_mouse_enter(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";
	if (!get_active()) {
		handled = true;
		return;
	}

	set_state2(FOCUSSED);
	handled = true;
}

void ttrack::signal_handler_mouse_leave(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";
	if (!get_active()) {
		handled = true;
		return;
	}
	set_state2(NORMAL);
	handled = true;
}

void ttrack::signal_handler_left_button_down(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	twindow* window = get_window();
	if (window && require_capture_) {
		window->mouse_capture();
	}

	set_state2(PRESSED);
	handled = true;
}

void ttrack::signal_handler_left_button_up(
		const event::tevent event, bool& handled)
{
	DBG_GUI_E << LOG_HEADER << ' ' << event << ".\n";

	set_state2(FOCUSSED);
	handled = true;
}

} // namespace gui2
